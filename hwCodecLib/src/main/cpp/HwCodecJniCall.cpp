#include <jni.h>
#include <string>
#include "LogUtils.h"
#include "AndroidThreadManager.h"

#include "ProcessExtractor.h"
#include "ProcessMuxer.h"
#include "ProcessDeCodec.h"


//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/hwcodeclib/ProcessHwCodec";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

ProcessExtractor *mProcessExtractor;
ProcessMuxer *mProcessMuxer;
ProcessDeCodec *mProcessDeCodec;

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

extern "C"
JNIEXPORT void JNICALL
cpp_process_hw_muxer(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath1,
                     jstring outPath2, jstring fmt) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath1 = env->GetStringUTFChars(outPath1, nullptr);
    const char *cOutPath2 = env->GetStringUTFChars(outPath2, nullptr);
    const char *cFmt = env->GetStringUTFChars(fmt, nullptr);

    if (mProcessMuxer == nullptr) {
        mProcessMuxer = new ProcessMuxer(env, thiz);
    }
    ThreadTask task = [cSrcPath, cOutPath1, cOutPath2, cFmt]() {
        mProcessMuxer->startProcessMuxer(cSrcPath, cOutPath1, cOutPath2, cFmt);
    };

    g_threadManager->submitTask("ProcessMuxerThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(outPath1, cOutPath1);
    env->ReleaseStringUTFChars(outPath2, cOutPath2);
    env->ReleaseStringUTFChars(fmt, cFmt);

}

extern "C"
JNIEXPORT void JNICALL
cpp_process_hw_decodec(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath1,
                       jstring outPath2, jstring codecName) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath1 = env->GetStringUTFChars(outPath1, nullptr);
    const char *cOutPath2 = env->GetStringUTFChars(outPath2, nullptr);
    const char *cCodecName = env->GetStringUTFChars(codecName, nullptr);

    if (mProcessDeCodec == nullptr) {
        mProcessDeCodec = new ProcessDeCodec(env, thiz);
    }
    ThreadTask task = [cSrcPath, cOutPath1, cOutPath2, cCodecName]() {
        mProcessDeCodec->startProcessDecodec(cSrcPath, cOutPath1, cOutPath2, cCodecName);
    };

    g_threadManager->submitTask("ProcessDecodecThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(srcPath, cSrcPath);
    env->ReleaseStringUTFChars(outPath1, cOutPath1);
    env->ReleaseStringUTFChars(outPath2, cOutPath2);
    env->ReleaseStringUTFChars(codecName, cCodecName);

}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_hw_codec_string_from_jni", "()Ljava/lang/String;", (void *) cpp_string_from_jni},
        {"native_process_hw_extractor",     "(Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_process_hw_extractor},
        {"native_process_hw_muxer",         "(Ljava/lang/String;"
                                            "Ljava/lang/String;"
                                            "Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_process_hw_muxer},
        {"native_process_hw_decodec",       "(Ljava/lang/String;"
                                            "Ljava/lang/String;"
                                            "Ljava/lang/String;"
                                            "Ljava/lang/String;)V", (void *) cpp_process_hw_decodec},
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