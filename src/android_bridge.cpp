#include "android_bridge.h"
#ifdef __ANDROID__
#include <SDL_system.h>
#include <jni.h>
namespace mcu {
namespace {
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
    }
    if (values)
        a.env->DeleteLocalRef(values);
    return out;
}
void androidChooseRom() {
    Activity a;
    a.env->CallVoidMethod(a.object, a.method("chooseRom", "()V"));
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
AndroidDisplay androidDisplay() { return {}; }
void androidChooseRom() {}
void androidOpenGuide(const char *, bool) {}
void androidPresentation(int, bool) {}
void androidVibrate() {}
} // namespace mcu
#endif
