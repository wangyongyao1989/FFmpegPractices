//
// Created by wangyao on 2025/9/22.
//

#include "includes/ProcessMuxer.h"


ProcessMuxer::ProcessMuxer(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

ProcessMuxer::~ProcessMuxer() {
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

    if (inputFp) {
        fclose(inputFp);
    }


}


void
ProcessMuxer::startProcessMuxer(const char *srcPath, const char *outPath1,
                                const char *outPat2, const char *fmt) {
    sSrcPath = srcPath;
    sOutPath1 = outPath1;
    sOutPath2 = outPat2;
    sFmt = fmt;
    LOGI("sSrcPath :%s \n sOutPath2: %s ", sSrcPath.c_str(), sOutPath2.c_str());
    callbackInfo =
            "sSrcPath:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());

    outputFormat = getMuxerOutFormat(sFmt);

    mHwMuxer = new HwMuxer();
    if (mHwMuxer == nullptr) {
        LOGE("Muxer creation failed ");
        callbackInfo =
                "Muxer creation failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    mHwExtractor = mHwMuxer->getExtractor();
    writeStatsHeader();
    processProcessMuxer();
}

void ProcessMuxer::processProcessMuxer() {
    inputFp = fopen(sSrcPath.c_str(), "rb");
    if (!inputFp) {
        LOGE("Unable to open :%s", sSrcPath.c_str());
        callbackInfo =
                "Unable to open " + sSrcPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    LOGI("Success open file :%s", sOutPath1.c_str());
    callbackInfo =
            "Success open file:" + sOutPath1 + "\n";
    PostStatusMessage(callbackInfo.c_str());

    // Read file properties
    struct stat buf;
    stat(sSrcPath.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t fd = fileno(inputFp);

    int32_t trackCount = mHwExtractor->initExtractor(fd, fileSize);
    if (trackCount < 0) {
        LOGE("initExtractor failed");
        callbackInfo = "initExtractor failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }
    LOGI("initExtractor Success trackCount: %d", trackCount);
    callbackInfo = "initExtractor Success trackCount:" + to_string(trackCount) + "\n";
    PostStatusMessage(callbackInfo.c_str());

    for (int curTrack = 0; curTrack < trackCount; curTrack++) {
        int32_t status = mHwExtractor->setupTrackFormat(curTrack);
        if (status != AMEDIA_OK) {
            LOGE("Track Format invalid");
            callbackInfo = "Track Format invalid \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        LOGI("curTrack : %d", curTrack);
        callbackInfo = "curTrack:" + to_string(curTrack) + "\n";
        PostStatusMessage(callbackInfo.c_str());

        uint8_t *inputBuffer = (uint8_t *) malloc(kMaxBufferSize);
        if (inputBuffer == nullptr) {
            LOGE("Insufficient memory");
            callbackInfo = "Insufficient memory \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        // AMediaCodecBufferInfo : <size of frame> <flags> <presentationTimeUs> <offset>
        vector<AMediaCodecBufferInfo> frameInfos;
        AMediaCodecBufferInfo info;
        uint32_t inputBufferOffset = 0;

        // Get Frame Data
        while (1) {
            status = mHwExtractor->getFrameSample(info);
            if (status || !info.size) break;
            // copy the meta data and buffer to be passed to muxer
            if (inputBufferOffset + info.size >= kMaxBufferSize) {
                LOGE("Memory allocated not sufficient");
                callbackInfo = "Memory allocated not sufficient \n";
                PostStatusMessage(callbackInfo.c_str());
                return;
            }
            memcpy(inputBuffer + inputBufferOffset, mHwExtractor->getFrameBuf(), info.size);
            info.offset = inputBufferOffset;
            frameInfos.push_back(info);
            inputBufferOffset += info.size;
        }

        string outputFileName = sOutPath2;
        FILE *outputFp = fopen(outputFileName.c_str(), "w+b");
        if (!outputFp) {
            LOGE("Unable to open output file :%s", outputFileName.c_str());
            callbackInfo =
                    "Unable to open output file:" + outputFileName + " for writing" + "\n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }

        LOGI("Success open file :%s", outputFileName.c_str());
        callbackInfo =
                "Success open file:" + outputFileName + "\n";
        PostStatusMessage(callbackInfo.c_str());

        int32_t fd = fileno(outputFp);
        status = mHwMuxer->initMuxer(fd, outputFormat);
        if (status != AMEDIA_OK) {
            LOGE("initMuxer failed");
            callbackInfo = "initMuxer failed \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        LOGI("initMuxer Success");
        callbackInfo = "initMuxer Success \n";
        PostStatusMessage(callbackInfo.c_str());

        status = mHwMuxer->mux(inputBuffer, frameInfos);
        if (status != AMEDIA_OK) {
            LOGE("Mux failed");
            callbackInfo = "Mux failed \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        LOGI("Mux Success");
        callbackInfo = "Mux Success \n";
        PostStatusMessage(callbackInfo.c_str());

        mHwMuxer->deInitMuxer();
        mHwMuxer->dumpStatistics(sSrcPath, sFmt, sOutPath1);
        free(inputBuffer);
        fclose(outputFp);
        mHwMuxer->resetMuxer();
    }
    LOGI("processProcessMuxer Success");
    fclose(inputFp);
    mHwExtractor->deInitExtractor();

}

MUXER_OUTPUT_T ProcessMuxer::getMuxerOutFormat(string fmt) {
    static const struct {
        string name;
        MUXER_OUTPUT_T value;
    } kFormatMaps[] = {{"mp4",  MUXER_OUTPUT_FORMAT_MPEG_4},
                       {"webm", MUXER_OUTPUT_FORMAT_WEBM},
                       {"3gpp", MUXER_OUTPUT_FORMAT_3GPP},
                       {"ogg",  MUXER_OUTPUT_FORMAT_OGG}};

    MUXER_OUTPUT_T format = MUXER_OUTPUT_FORMAT_INVALID;
    for (size_t i = 0; i < sizeof(kFormatMaps) / sizeof(kFormatMaps[0]); ++i) {
        if (!fmt.compare(kFormatMaps[i].name)) {
            format = kFormatMaps[i].value;
            break;
        }
    }
    return format;
}


bool ProcessMuxer::writeStatsHeader() {
    char statsHeader[] =
            "currentTime, fileName, operation, componentName, NDK/SDK, sync/async, setupTime, "
            "destroyTime, minimumTime, maximumTime, averageTime, timeToProcess1SecContent, "
            "totalBytesProcessedPerSec, timeToFirstFrame, totalSizeInBytes, totalTime\n";
    FILE *fpStats = fopen(sOutPath1.c_str(), "w");
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


JNIEnv *ProcessMuxer::GetJNIEnv(bool *isAttach) {
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

void ProcessMuxer::PostStatusMessage(const char *msg) {
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