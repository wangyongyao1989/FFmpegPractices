#include <jni.h>
#include <string>
#include "LogUtils.h"
#include "AndroidThreadManager.h"

#include "ProcessExtractor.h"


//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/hwcodeclib/ProcessHwCodec";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

ProcessExtractor *mProcessExtractor;

extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "cpp_string_from_jni: ");
    LOGD("cpp_string_from_jni\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT void JNICALL
cpp_process_hw_extractor(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mProcessExtractor == nullptr) {
        mProcessExtractor = new ProcessExtractor(env, thiz);
    }
    ThreadTask task = [cSrcPath, cOutPath]() {
        mProcessExtractor->startProcessExtractor(cSrcPath, cOutPath);
    };

    g_threadManager->submitTask("ProcessExtractorThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);

}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_hw_codec_string_from_jni", "()Ljava/lang/String;", (void *) cpp_string_from_jni},
        {"native_process_hw_extractor",     "(Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_process_hw_extractor},
};




/**
 * 定义注册方法
 * @param vm
 * @param reserved
 * @return
 */
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    LOGD("动态注册");
    JNIEnv *env;
    if ((vm)->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        LOGD("动态注册GetEnv  fail");
        return JNI_ERR;
    }

    g_jvm = vm;
    g_threadManager = std::make_unique<AndroidThreadManager>(vm);

    // 初始化线程池
    ThreadPoolConfig config;
    config.minThreads = 2;
    config.maxThreads = 4;
    config.idleTimeoutMs = 30000;
    config.queueSize = 50;
    g_threadManager->initThreadPool(config);

    // 获取类引用
    jclass clazz = env->FindClass(java_class_name);
    // 注册native方法
    jint regist_result = env->RegisterNatives(clazz, methods,
                                              sizeof(methods) / sizeof(methods[0]));
    if (regist_result) { // 非零true 进if
        LOGE("动态注册 fail regist_result = %d", regist_result);
    } else {
        LOGI("动态注册 success result = %d", regist_result);
    }
    return JNI_VERSION_1_6;
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    g_threadManager.reset();
    if (mProcessExtractor) {
        mProcessExtractor = nullptr;
    }
}