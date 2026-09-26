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
    // 成员指针每点一次按钮就被覆盖一次，上一个实例（连同它持有的 AMediaCodec /
    // AMediaExtractor 等 native 句柄）再也拿不到，只能等进程退出。先回收上一轮。
    if (mHwExtractor != nullptr) {
        delete mHwExtractor;
        mHwExtractor = nullptr;
    }
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
        // inputFp 是成员，早退不关就等于每失败一次泄漏一个 fd：
        // 下一次 fopen 会把成员覆盖掉，这个 FILE* 再也没人释放。
        fclose(inputFp);
        inputFp = nullptr;
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
        fclose(inputFp);
        inputFp = nullptr;
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
        callbackInfo = "video mime_type:" + string(video_mime_type ? video_mime_type : "null") + "\n";
        // AMediaFormat_getString 填出来的指针归 AMediaFormat 对象所有，
        // 调用方 delete 它是在释放非 new 申请的内存（堆破坏，表现为随机崩溃）。
        video_mime_type = nullptr;

        // AMediaFormat_getInt32 取不到 key 时返回 false 且不写出参，
        // 原来的 int32_t 全是未初始化变量，界面上就打出了栈残值
        // （真机日志里 video color_format/bit_rate/i_frame_rate、audio frame_rate
        //  同时打成 122 就是这么来的）。现在统一给 0 并标注是否真的有值。
        int32_t width = 0;
        bool widthFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_WIDTH, &width);
        LOGI("video width: %d%s", width, widthFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video width:" + to_string(width) +
                       (widthFound ? "" : "(未提供)") + "\n";

        int32_t height = 0;
        bool heightFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_HEIGHT, &height);
        LOGI("video height: %d%s", height, heightFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video height:" + to_string(height) +
                       (heightFound ? "" : "(未提供)") + "\n";

        int32_t color_format = 0;
        bool colorFormatFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                                      &color_format);
        LOGI("video color_format: %d%s", color_format, colorFormatFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video color_format:" + to_string(color_format) +
                       (colorFormatFound ? "" : "(未提供)") + "\n";

        int32_t bit_rate = 0;
        bool bitRateFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bit_rate);
        LOGI("video bit_rate: %d%s", bit_rate, bitRateFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video bit_rate:" + to_string(bit_rate) +
                       (bitRateFound ? "" : "(未提供)") + "\n";

        int32_t frame_rate = 0;
        bool frameRateFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_FRAME_RATE,
                                                    &frame_rate);
        LOGI("video frame_rate: %d%s", frame_rate, frameRateFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video frame_rate:" + to_string(frame_rate) +
                       (frameRateFound ? "" : "(未提供)") + "\n";

        int32_t i_frame_interval = 0;
        bool iFrameFound = AMediaFormat_getInt32(videoFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL,
                                                 &i_frame_interval);
        // 这个 key 是编码器的 I 帧间隔（秒），不是帧率，名字改成 i_frame_interval
        LOGI("video i_frame_interval: %d%s", i_frame_interval, iFrameFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "video i_frame_interval:" + to_string(i_frame_interval) +
                       (iFrameFound ? "" : "(未提供)") + "\n";
        PostStatusMessage(callbackInfo.c_str());
    }

    //选择音频轨打印出相关参数
    mHwExtractor->setupTrackFormat(1);
    AMediaFormat *audioFormat = mHwExtractor->getFormat();
    if (audioFormat) {
        const char *audio_mime_type = nullptr;
        AMediaFormat_getString(audioFormat, AMEDIAFORMAT_KEY_MIME, &audio_mime_type);
        LOGI("audio mime_type: %s", audio_mime_type);
        callbackInfo = "audio mime_type:" + string(audio_mime_type ? audio_mime_type : "null") + "\n";
        // AMediaFormat_getString 填出来的指针归 AMediaFormat 对象所有，
        // 调用方 delete 它是在释放非 new 申请的内存（堆破坏，表现为随机崩溃）。
        audio_mime_type = nullptr;

        // 同视频轨：取不到 key 时出参不被写，未初始化变量会把栈残值打到界面上。
        int32_t frame_rate = 0;
        bool frameRateFound = AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_FRAME_RATE,
                                                    &frame_rate);
        LOGI("audio frame_rate: %d%s", frame_rate, frameRateFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "audio frame_rate:" + to_string(frame_rate) +
                       (frameRateFound ? "" : "(未提供)") + "\n";

        int32_t bit_rate = 0;
        bool bitRateFound = AMediaFormat_getInt32(audioFormat, AMEDIAFORMAT_KEY_BIT_RATE, &bit_rate);
        LOGI("audio bit_rate: %d%s", bit_rate, bitRateFound ? "" : "(未提供)");
        callbackInfo = callbackInfo + "audio bit_rate:" + to_string(bit_rate) +
                       (bitRateFound ? "" : "(未提供)") + "\n";

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