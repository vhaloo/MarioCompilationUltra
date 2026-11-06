#include "android_bridge.h"
#include "watch.h"
#include <cmath>
#include <algorithm>
#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <mutex>
namespace mcu {
namespace {
std::atomic<uint32_t> pauseRequested{0};
std::mutex pauseMutex;
std::condition_variable pauseFinished;
uint32_t pauseCompleted = 0;
struct Activity {
    JNIEnv *env = static_cast<JNIEnv *>(SDL_AndroidGetJNIEnv());
    jobject object = static_cast<jobject>(SDL_AndroidGetActivity());
    jclass type = env->GetObjectClass(object);
    ~Activity() {
        env->DeleteLocalRef(type);
        env->DeleteLocalRef(object);
    }
    jmethodID method(const char *name, const char *signature) {
        return env->GetMethodID(type, name, signature);
    }
};
} // namespace
void androidCheckpointComplete(uint32_t ticket) {
    {
        std::lock_guard<std::mutex> lock(pauseMutex);
        pauseCompleted = std::max(pauseCompleted, ticket);
    }
    pauseFinished.notify_all();
}
extern "C" JNIEXPORT void JNICALL
Java_com_vhaloo_mariocompilationultra_android_HandheldActivity_nativePreparePause(JNIEnv *,
                                                                                  jclass) {
    if (!SDL_WasInit(SDL_INIT_EVENTS))
        return;
    const uint32_t ticket = ++pauseRequested;
    SDL_Event event{};
    event.type = SDL_USEREVENT;
    event.user.code = AndroidCheckpointEvent;
    event.user.data1 = reinterpret_cast<void *>(uintptr_t(ticket));
    if (SDL_PushEvent(&event) != 1)
        return;
    // Checkpoint on the emulation thread before Java lets SDL suspend it.
    std::unique_lock<std::mutex> lock(pauseMutex);
    pauseFinished.wait_for(lock, std::chrono::milliseconds(800),
                           [&] { return pauseCompleted >= ticket; });
}
AndroidDisplay androidDisplay() {
    Activity a;
    AndroidDisplay out;
    auto method = a.method("getLayoutMetrics", "()[I");
    auto values = static_cast<jintArray>(a.env->CallObjectMethod(a.object, method));
    if (values && a.env->GetArrayLength(values) >= 5) {
        jint v[5];
        a.env->GetIntArrayRegion(values, 0, 5, v);
        out.density = v[0] / 1000.f;
        out.insets = {v[1], v[2], v[3], v[4]};
        if (a.env->GetArrayLength(values) >= 9) {
            jint profile[4];
            a.env->GetIntArrayRegion(values, 5, 4, profile);
            out.watch = profile[0] != 0;
            out.round = profile[1] != 0;
            out.rotary = profile[2] != 0;
            out.thermal = std::clamp(int(profile[3]), 0, 6);
        }
    }
    if (values)
        a.env->DeleteLocalRef(values);
    return out;
}
int androidThermalStatus() {
    Activity a;
    return std::clamp(int(a.env->CallIntMethod(a.object, a.method("getThermalStatus", "()I"))), 0,
                      6);
}
extern "C" JNIEXPORT void JNICALL
Java_com_vhaloo_mariocompilationultra_android_HandheldActivity_nativeRotary(JNIEnv *, jclass,
                                                                            jfloat delta) {
    if (!std::isfinite(delta))
        return;
    SDL_Event event{};
    event.type = SDL_USEREVENT;
    event.user.code = RotaryEvent;
    event.user.data1 =
        reinterpret_cast<void *>(intptr_t(std::lround(std::clamp(delta, -10.f, 10.f) * 1000)));
    SDL_PushEvent(&event);
}
void androidChooseRom(const char *language) {
    Activity a;
    auto code = a.env->NewStringUTF(language);
    a.env->CallVoidMethod(a.object, a.method("chooseRom", "(Ljava/lang/String;)V"), code);
    a.env->DeleteLocalRef(code);
}
void androidCheckInbox(const char *language) {
    Activity a;
    auto code = a.env->NewStringUTF(language);
    a.env->CallVoidMethod(a.object, a.method("checkTransferredCartridges", "(Ljava/lang/String;)V"),
                          code);
    a.env->DeleteLocalRef(code);
}
void androidOpenGuide(const char *language, bool mobile) {
    Activity a;
    auto s = a.env->NewStringUTF(language);
    a.env->CallVoidMethod(a.object, a.method("openGuide", "(Ljava/lang/String;Z)V"), s,
                          jboolean(mobile));
    a.env->DeleteLocalRef(s);
}
void androidPresentation(int orientation, bool awake) {
    Activity a;
    a.env->CallVoidMethod(a.object, a.method("configurePresentation", "(IZ)V"), jint(orientation),
                          jboolean(awake));
}
void androidVibrate() {
    Activity a;
    a.env->CallVoidMethod(a.object, a.method("tapFeedback", "()V"));
}
} // namespace mcu
#else
namespace mcu {
void androidCheckpointComplete(uint32_t) {}
AndroidDisplay androidDisplay() { return {}; }
int androidThermalStatus() { return 0; }
void androidChooseRom(const char *) {}
void androidCheckInbox(const char *) {}
void androidOpenGuide(const char *, bool) {}
void androidPresentation(int, bool) {}
void androidVibrate() {}
} // namespace mcu
#endif
