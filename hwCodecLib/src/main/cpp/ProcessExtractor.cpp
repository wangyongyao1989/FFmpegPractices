//
// Created by wangyao on 2025/9/21.
//

#include "includes/ProcessExtractor.h"


ProcessExtractor::ProcessExtractor(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

ProcessExtractor::~ProcessExtractor() {
    mEnv->DeleteGlobalRef(mJavaObj);
    if (mEnv) {
        mEnv = nullptr;
    }

    if (mJavaVm) {
        mJavaVm = nullptr;
    }

    if (mJavaObj) {
        mJavaObj = nullptr;
    }

    if (mHwExtractor) {
        mHwExtractor->deInitExtractor();
        mHwExtractor = nullptr;
    }

    if (inputFp) {
        fclose(inputFp);
    }

}


void ProcessExtractor::startProcessExtractor(const char *srcPath, const char *outPath) {
    sSrcPath = srcPath;
    sOutPath = outPath;
    LOGI("sSrcPath :%s \n sOutPath: %s ", sSrcPath.c_str(), sOutPath.c_str());
    callbackInfo =
            "sSrcPath:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());
    mHwExtractor = new HwExtractor();
    if (mHwExtractor == nullptr) {
        LOGE("Extractor creation failed ");
        callbackInfo =
                "Extractor creation failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }
    LOGI("Extractor creation Success!");
    processProcessExtractor();
}

void ProcessExtractor::processProcessExtractor() {
    inputFp = fopen(sSrcPath.c_str(), "rb");
    if (!inputFp) {
        LOGE("Unable to open :%s", sSrcPath.c_str());
        callbackInfo =
                "Unable to open " + sSrcPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    LOGI("Success open file :%s", sSrcPath.c_str());
    callbackInfo =
            "Success open file:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());

    // Read file properties
    struct stat buf;
    stat(sSrcPath.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t fd = fileno(inputFp);
    int32_t trackCount = mHwExtractor->initExtractor((long) fd, fileSize);

    if (trackCount < 0) {
        LOGE("initExtractor failed");
        callbackInfo = "initExtractor failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }
    LOGI("initExtractor Success");
    callbackInfo = "initExtractor Success \n";
    PostStatusMessage(callbackInfo.c_str());

    int32_t trackID = 1;
    int32_t status = mHwExtractor->extract(trackID);
    if (status != AMEDIA_OK) {
        LOGE("Extraction failed");
        callbackInfo = "Extraction failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }
    LOGI("Extraction Success");
    callbackInfo = "Extraction Success \n";
    PostStatusMessage(callbackInfo.c_str());

    //选择视频轨打印出相关参数
    mHwExtractor->setupTrackFormat(0);
    AMediaFormat *videoFormat = mHwExtractor->getFormat();
    if (videoFormat) {
        const char *video_mime_type = nullptr;
        AMediaFormat_getString(videoFormat, AMEDIAFORMAT_KEY_MIME, &video_mime_type);
        LOGI("video mime_type: %s", video_mime_type);
        callbackInfo = "video mime_type:" + string(video_mime_type) + "\n";
        delete (video_mime_type);
        video_mime_type = nullptr;

        int32_t width;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, &width);
        LOGI("video width: %d", width);
        callbackInfo = callbackInfo + "video width:" + to_string(width) + "\n";

        int32_t height;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, &height);
        LOGI("video height: %d", height);
        callbackInfo = callbackInfo + "video height:" + to_string(height) + "\n";

        int32_t color_format;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, &color_format);
        LOGI("video color_format: %d", color_format);
        callbackInfo = callbackInfo + "video color_format:" + to_string(color_format) + "\n";

        int32_t bit_rate;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bit_rate);
        LOGI("video bit_rate: %d", bit_rate);
        callbackInfo = callbackInfo + "video bit_rate:" + to_string(bit_rate) + "\n";

        int32_t frame_rate;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_FRAME_RATE, &frame_rate);
        LOGI("video frame_rate: %d", frame_rate);
        callbackInfo = callbackInfo + "video frame_rate:" + to_string(frame_rate) + "\n";

        int32_t i_frame_rate;
        AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, &i_frame_rate);
        LOGI("video i_frame_rate: %d", i_frame_rate);
        callbackInfo = callbackInfo + "video i_frame_rate:" + to_string(i_frame_rate) + "\n";
        PostStatusMessage(callbackInfo.c_str());
    }

    //选择音频轨打印出相关参数
    mHwExtractor->setupTrackFormat(1);
    AMediaFormat *audioFormat = mHwExtractor->getFormat();
    if (audioFormat) {
        const char *audio_mime_type = nullptr;
        AMediaFormat_getString(audioFormat, AMEDIAFORMAT_KEY_MIME, &audio_mime_type);
        LOGI("audio mime_type: %s", audio_mime_type);
        callbackInfo = "audio mime_type:" + string(audio_mime_type) + "\n";
        delete (audio_mime_type);
        audio_mime_type = nullptr;

        int32_t frame_rate;
        AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_FRAME_RATE, &frame_rate);

        LOGI("audio frame_rate: %d", frame_rate);
        callbackInfo = callbackInfo + "audio frame_rate:" + to_string(frame_rate) + "\n";

        int32_t bit_rate;
        AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bit_rate);
        LOGI("audio bit_rate: %d", bit_rate);
        callbackInfo = callbackInfo + "audio bit_rate:" + to_string(bit_rate) + "\n";

        PostStatusMessage(callbackInfo.c_str());
    }

    bool writeStat = writeStatsHeader();
    mHwExtractor->deInitExtractor();
    mHwExtractor->dumpStatistics(sSrcPath, "", sOutPath);

    LOGI("dumpStatistics Success");
    callbackInfo = "dumpStatistics Success file:" + sOutPath + "\n";
    PostStatusMessage(callbackInfo.c_str());

    fclose(inputFp);
}


bool ProcessExtractor::writeStatsHeader() {
    char statsHeader[] =
            "currentTime, fileName, operation, componentName, NDK/SDK, sync/async, setupTime, "
            "destroyTime, minimumTime, maximumTime, averageTime, timeToProcess1SecContent, "
            "totalBytesProcessedPerSec, timeToFirstFrame, totalSizeInBytes, totalTime\n";
    FILE *fpStats = fopen(sOutPath.c_str(), "w");
    if (!fpStats) {
        return false;
    }
    int32_t numBytes = fwrite(statsHeader, sizeof(char), sizeof(statsHeader), fpStats);
    fclose(fpStats);
    if (numBytes != sizeof(statsHeader)) {
        return false;
    }
    return true;
}


JNIEnv *ProcessExtractor::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("SaveYUVFromVideo::GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("SaveYUVFromVideo::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void ProcessExtractor::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }
    jobject javaObj = mJavaObj;
    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(javaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    jstring pJstring = pEnv->NewStringUTF(msg);
    pEnv->CallVoidMethod(javaObj, mid, pJstring);
    if (isAttach) {
        JavaVM *pJavaVm = mJavaVm;
        pJavaVm->DetachCurrentThread();
    }
}