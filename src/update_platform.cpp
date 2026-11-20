#include "update.h"
#include <fstream>
#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
#elif defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#include <winhttp.h>
#else
#include <curl/curl.h>
#include <spawn.h>
#include <unistd.h>
#ifdef __APPLE__
#include <crt_externs.h>
#else
extern char **environ;
#endif
#endif

namespace mcu {
#ifdef __ANDROID__
namespace {
struct ActivityCall {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject object = env ? static_cast<jobject>(SDL_AndroidGetActivity()) : nullptr;
    jclass type = object ? env->GetObjectClass(object) : nullptr;
    ~ActivityCall() {
        if (type)
            env->DeleteLocalRef(type);
        if (object)
            env->DeleteLocalRef(object);
    }
    bool run(const char *name, const char *signature, const std::string &a, const std::string &b,
             jlong limit = 0) {
        if (!type)
            return false;
        jmethodID method = env->GetMethodID(type, name, signature);
        auto first = env->NewStringUTF(a.c_str()), second = env->NewStringUTF(b.c_str());
        bool ok = method && (limit ? env->CallBooleanMethod(object, method, first, second, limit)
                                   : env->CallBooleanMethod(object, method, first, second));
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            ok = false;
        }
        env->DeleteLocalRef(first);
        env->DeleteLocalRef(second);
        return ok;
    }
};
} // namespace
bool fetchUpdateFile(const std::string &url, const fs::path &out, size_t limit, bool,
                     const std::atomic<bool> &stop) {
    return !stop &&
           ActivityCall().run("fetchSoftwareUpdate", "(Ljava/lang/String;Ljava/lang/String;J)Z",
                              url, out.u8string(), limit);
}
bool installAndroidUpdate(const fs::path &p, const std::string &locale) {
    return ActivityCall().run("installSoftwareUpdate", "(Ljava/lang/String;Ljava/lang/String;)Z",
                              p.u8string(), locale);
}
bool launchUpdatedProgram(const fs::path &, const std::vector<std::string> &) { return false; }
#elif defined(_WIN32)
namespace {
struct Http {
    HINTERNET h;
    ~Http() {
        if (h)
            WinHttpCloseHandle(h);
    }
};
std::wstring quote(const std::wstring &s) {
    std::wstring out = L"\"";
    size_t n = 0;
    for (wchar_t c : s) {
        if (c == L'\\') {
            ++n;
            continue;
        }
        out.append(c == L'"' ? 2 * n + 1 : n, L'\\');
        n = 0;
        out += c;
    }
    out.append(2 * n, L'\\');
    return out + L'"';
}
} // namespace
bool fetchUpdateFile(const std::string &url, const fs::path &out, size_t limit, bool package,
                     const std::atomic<bool> &stop) {
    std::wstring wide(url.begin(), url.end());
    URL_COMPONENTS parts{};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = parts.dwUrlPathLength = parts.dwExtraInfoLength = DWORD(-1);
    if (!WinHttpCrackUrl(wide.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS)
        return false;
    std::wstring host(parts.lpszHostName, parts.dwHostNameLength),
        path(parts.lpszUrlPath, parts.dwUrlPathLength);
    if (parts.dwExtraInfoLength)
        path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    Http session{WinHttpOpen(L"MarioCompilationUltra-Updater/1",
                             WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, nullptr, nullptr, 0)};
    if (!session.h)
        return false;
    WinHttpSetTimeouts(session.h, 3000, 5000, 5000, package ? 15000 : 6000);
    Http connection{WinHttpConnect(session.h, host.c_str(), parts.nPort, 0)};
    Http req{connection.h ? WinHttpOpenRequest(connection.h, L"GET", path.c_str(), nullptr, nullptr,
                                               nullptr, WINHTTP_FLAG_SECURE)
                          : nullptr};
    DWORD redirects = package ? WINHTTP_OPTION_REDIRECT_POLICY_DISALLOW_HTTPS_TO_HTTP
                              : WINHTTP_OPTION_REDIRECT_POLICY_NEVER;
    if (!req.h ||
        !WinHttpSetOption(req.h, WINHTTP_OPTION_REDIRECT_POLICY, &redirects, sizeof(redirects)) ||
        !WinHttpSendRequest(req.h, WINHTTP_NO_ADDITIONAL_HEADERS, 0, nullptr, 0, 0, 0) ||
        !WinHttpReceiveResponse(req.h, nullptr))
        return false;
    DWORD code = 0, sz = sizeof(code);
    if (!WinHttpQueryHeaders(req.h, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, nullptr,
                             &code, &sz, nullptr) ||
        code != 200)
        return false;
    std::ofstream f(out, std::ios::binary);
    char buf[32768];
    size_t total = 0;
    while (!stop && f) {
        DWORD n = 0;
        if (!WinHttpReadData(req.h, buf, sizeof(buf), &n))
            return false;
        if (!n) {
            f.close();
            return bool(f);
        }
        if (n > limit - total)
            return false;
        total += n;
        f.write(buf, n);
    }
    return false;
}
bool launchUpdatedProgram(const fs::path &exe, const std::vector<std::string> &args) {
    std::wstring cmd = quote(exe.wstring());
    for (const auto &a : args)
        cmd += L" " + quote(fs::u8path(a).wstring());
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(exe.c_str(), cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, nullptr, &si,
                        &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
#else
namespace {
struct Transfer {
    std::ofstream out;
    size_t remaining;
    const std::atomic<bool> &stop;
};
size_t writeBytes(char *data, size_t unit, size_t count, void *opaque) {
    auto &t = *static_cast<Transfer *>(opaque);
    if (t.stop || (unit && count > t.remaining / unit))
        return 0;
    size_t n = unit * count;
    t.out.write(data, n);
    t.remaining -= n;
    return t.out ? n : 0;
}
} // namespace
bool fetchUpdateFile(const std::string &url, const fs::path &out, size_t limit, bool package,
                     const std::atomic<bool> &stop) {
    static const auto initialized = curl_global_init(CURL_GLOBAL_DEFAULT);
    if (initialized != CURLE_OK)
        return false;
    auto *curl = curl_easy_init();
    if (!curl)
        return false;
    Transfer t{std::ofstream(out, std::ios::binary), limit, stop};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "MarioCompilationUltra-Updater/1");
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 5L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, package ? 180L : 9L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, package ? 1L : 0L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 5L);
    curl_easy_setopt(curl, CURLOPT_REDIR_PROTOCOLS, long(CURLPROTO_HTTPS));
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeBytes);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &t);
    auto res = curl_easy_perform(curl);
    long code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &code);
    curl_easy_cleanup(curl);
    t.out.close();
    return res == CURLE_OK && code == 200 && bool(t.out);
}
bool launchUpdatedProgram(const fs::path &exe, const std::vector<std::string> &args) {
    std::string path = exe.string();
    std::vector<char *> argv{path.data()};
    for (auto &arg : args)
        argv.push_back(const_cast<char *>(arg.c_str()));
    argv.push_back(nullptr);
#ifdef __APPLE__
    char **environment = *_NSGetEnviron();
#else
    char **environment = environ;
#endif
    pid_t child = 0;
    return posix_spawn(&child, path.c_str(), nullptr, nullptr, argv.data(), environment) == 0;
}
#endif
#ifndef __ANDROID__
bool installAndroidUpdate(const fs::path &, const std::string &) { return false; }
#endif
} // namespace mcu
