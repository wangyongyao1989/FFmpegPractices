//
// Created by wangyao on 2025/8/31.
//
#include <jni.h>
#include <string>
#include "BasicCommon.h"
#include "WriteYUVFrame.h"
#include "SaveYUVFromVideo.h"
#include "SaveJPGFromVideo.h"
#include "AndroidThreadManager.h"
#include "SaveJPGSwsFromVideo.h"
#include "SavePNGSwsFromVideo.h"
#include "SaveBMPSwsFromVideo.h"
#include "SaveGifSwsOfVideo.h"
#include "SaveImage2Video.h"

//包名+类名字符串定义：
const char *java_class_name = "com/wangyao/processimagelib/ProcessImageOperate";
using namespace std;

JavaVM *g_jvm = nullptr;
std::unique_ptr<AndroidThreadManager> g_threadManager;

WriteYUVFrame *mWriteFrame;
SaveYUVFromVideo *mSaveYUVFromVideo;
SaveJPGFromVideo *mSaveJPGFromVideo;
SaveJPGSwsFromVideo *mSaveJPGSwsFromVideo;
SavePNGSwsFromVideo *mSavePNGSwsFromVideo;
SaveBMPSwsFromVideo *mSaveBMPSwsFromVideo;
SaveGifSwsOfVideo *mSaveGifOfVideo;
SaveImage2Video *mSaveImage2Video;

extern "C"
JNIEXPORT jstring JNICALL
cpp_string_from_jni(JNIEnv *env, jobject thiz) {
    char strBuffer[1024 * 4] = {0};
    strcat(strBuffer, "libavcodec : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVCODEC_VERSION));
    strcat(strBuffer, "\nlibavformat : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFORMAT_VERSION));
    strcat(strBuffer, "\nlibavutil : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVUTIL_VERSION));
    strcat(strBuffer, "\nlibavfilter : ");
    strcat(strBuffer, AV_STRINGIFY(LIBAVFILTER_VERSION));
    strcat(strBuffer, "\nlibswresample : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWRESAMPLE_VERSION));
    strcat(strBuffer, "\nlibswscale : ");
    strcat(strBuffer, AV_STRINGIFY(LIBSWSCALE_VERSION));
    strcat(strBuffer, "\navcodec_configure : \n");
    strcat(strBuffer, avcodec_configuration());
    strcat(strBuffer, "\navcodec_license : ");
    strcat(strBuffer, avcodec_license());
    LOGD("GetFFmpegVersion\n%s", strBuffer);
    return env->NewStringUTF(strBuffer);
}

extern "C"
JNIEXPORT void JNICALL
cpp_write_yuv(JNIEnv *env, jobject thiz, jstring outPath) {
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mWriteFrame == nullptr) {
        mWriteFrame = new WriteYUVFrame(env, thiz);
    }

    mWriteFrame->startWriteYUVThread(cOutPath);

    env->ReleaseStringUTFChars(outPath, cOutPath);

}

extern "C"
JNIEXPORT void JNICALL
cpp_save_yuv_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveYUVFromVideo == nullptr) {
        mSaveYUVFromVideo = new SaveYUVFromVideo(env, thiz);
    }

    mSaveYUVFromVideo->startWriteYUVThread(cSrcPath, cOutPath);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_jpg_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveJPGFromVideo == nullptr) {
        mSaveJPGFromVideo = new SaveJPGFromVideo(env, thiz);
    }

    const std::string own_cSrcPath = cSrcPath ? cSrcPath : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath, own_cOutPath]() {
        mSaveJPGFromVideo->startWriteJPGThread(own_cSrcPath.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("WriteJPGThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_jpg_sws_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveJPGSwsFromVideo == nullptr) {
        mSaveJPGSwsFromVideo = new SaveJPGSwsFromVideo(env, thiz);
    }

    const std::string own_cSrcPath = cSrcPath ? cSrcPath : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath, own_cOutPath]() {
        mSaveJPGSwsFromVideo->startWriteJPGSws(own_cSrcPath.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("WriteJPGThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_png_sws_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSavePNGSwsFromVideo == nullptr) {
        mSavePNGSwsFromVideo = new SavePNGSwsFromVideo(env, thiz);
    }

    const std::string own_cSrcPath = cSrcPath ? cSrcPath : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath, own_cOutPath]() {
        mSavePNGSwsFromVideo->startWritePNGSws(own_cSrcPath.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("WritePNGThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_bmp_sws_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveBMPSwsFromVideo == nullptr) {
        mSaveBMPSwsFromVideo = new SaveBMPSwsFromVideo(env, thiz);
    }

    const std::string own_cSrcPath = cSrcPath ? cSrcPath : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath, own_cOutPath]() {
        mSaveBMPSwsFromVideo->startWriteBMPSws(own_cSrcPath.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("WriteBMPThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}

extern "C"
JNIEXPORT void JNICALL
cpp_save_gif_from_video(JNIEnv *env, jobject thiz, jstring srcPath, jstring outPath) {
    const char *cSrcPath = env->GetStringUTFChars(srcPath, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveGifOfVideo == nullptr) {
        mSaveGifOfVideo = new SaveGifSwsOfVideo(env, thiz);
    }

    const std::string own_cSrcPath = cSrcPath ? cSrcPath : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath, own_cOutPath]() {
        mSaveGifOfVideo->startWriteGif(own_cSrcPath.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("WriteGIFThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath, cSrcPath);
}
extern "C"
JNIEXPORT void JNICALL
cpp_save_image_to_video(JNIEnv *env, jobject thiz, jstring srcPath1,
                           jstring srcPath2, jstring outPath) {
    const char *cSrcPath1 = env->GetStringUTFChars(srcPath1, nullptr);
    const char *cSrcPath2 = env->GetStringUTFChars(srcPath2, nullptr);
    const char *cOutPath = env->GetStringUTFChars(outPath, nullptr);

    if (mSaveImage2Video == nullptr) {
        mSaveImage2Video = new SaveImage2Video(env, thiz);
    }

    const std::string own_cSrcPath1 = cSrcPath1 ? cSrcPath1 : "";
    const std::string own_cSrcPath2 = cSrcPath2 ? cSrcPath2 : "";
    const std::string own_cOutPath = cOutPath ? cOutPath : "";
    ThreadTask task = [own_cSrcPath1, own_cSrcPath2, own_cOutPath]() {
        mSaveImage2Video->startImage2Video(own_cSrcPath1.c_str(), own_cSrcPath2.c_str(), own_cOutPath.c_str());
    };

    g_threadManager->submitTask("Image2VideoThread", task, PRIORITY_NORMAL);

    env->ReleaseStringUTFChars(outPath, cOutPath);
    env->ReleaseStringUTFChars(srcPath1, cSrcPath1);
    env->ReleaseStringUTFChars(srcPath2, cSrcPath2);

}


// 重点：定义类名和函数签名，如果有多个方法要动态注册，在数组里面定义即可
static const JNINativeMethod methods[] = {
        {"native_string_from_jni",         "()Ljava/lang/String;",  (void *) cpp_string_from_jni},
        {"native_write_yuv",               "(Ljava/lang/String;)V", (void *) cpp_write_yuv},
        {"native_save_yuv_from_video",     "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_yuv_from_video},
        {"native_save_jpg_from_video",     "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_jpg_from_video},
        {"native_save_jpg_sws_from_video", "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_jpg_sws_from_video},
        {"native_save_png_sws_from_video", "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_png_sws_from_video},
        {"native_save_bmp_sws_from_video", "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_bmp_sws_from_video},
        {"native_save_gif_from_video",     "(Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_gif_from_video},
        {"native_save_image_to_video",     "(Ljava/lang/String;"
                                           "Ljava/lang/String;"
                                           "Ljava/lang/String;)V",  (void *) cpp_save_image_to_video},
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
    if (mWriteFrame) {
        mWriteFrame = nullptr;
    }
    if (mSaveYUVFromVideo) {
        mSaveYUVFromVideo = nullptr;
    }
    if (mSaveJPGFromVideo) {
        mSaveJPGFromVideo = nullptr;
    }
    if (mSaveJPGSwsFromVideo) {
        mSaveJPGSwsFromVideo = nullptr;
    }
    if (mSavePNGSwsFromVideo) {
        mSavePNGSwsFromVideo = nullptr;
    }
    if (mSaveBMPSwsFromVideo) {
        mSaveBMPSwsFromVideo = nullptr;
    }
    if (mSaveGifOfVideo) {
        mSaveGifOfVideo = nullptr;
    }
    if (mSaveImage2Video) {
        mSaveImage2Video = nullptr;
    }

}