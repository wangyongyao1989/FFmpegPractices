//
// Created by wangyao on 2025/9/24.
//

#include "includes/ProcessEnCodec.h"

ProcessEnCodec::ProcessEnCodec(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

ProcessEnCodec::~ProcessEnCodec() {
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

    if (outputFp) {
        fclose(outputFp);
    }

}

void ProcessEnCodec::startProcessEnCodec(const char *srcPath, const char *outPath1,
                                         const char *outPath2, const char *codecName) {
    sSrcPath = srcPath;
    sOutPath1 = outPath1;
    sOutPath2 = outPath2;
    sCodecName = codecName;

    LOGI("sSrcPath :%s \n sOutPath2: %s ", sSrcPath.c_str(), sOutPath2.c_str());
    callbackInfo =
            "sSrcPath:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());

    pHwDeCodec = new HwDeCodec();
    if (pHwDeCodec == nullptr) {
        LOGE("HwEnCodec creation failed ");
        callbackInfo =
                "HwEnCodec creation failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    pHwEnCodec = new HwEnCodec();
    if (pHwEnCodec == nullptr) {
        LOGE("HwEnCodec creation failed ");
        callbackInfo =
                "HwEnCodec creation failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    mHwExtractor = pHwDeCodec->getExtractor();

    if (mHwExtractor == nullptr) {
        LOGE("HwExtractor creation failed ");
        callbackInfo =
                "HwExtractor creation failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    writeStatsHeader();
    processProcessEnCodec();
}

void ProcessEnCodec::processProcessEnCodec() {
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

    for (int curTrack = 0; curTrack < trackCount; curTrack++) {
        if (curTrack == 1) break;
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

        vector<AMediaCodecBufferInfo> frameInfo;
        AMediaCodecBufferInfo info;
        uint32_t inputBufferOffset = 0;

        // Get frame data
        while (1) {
            status = mHwExtractor->getFrameSample(info);
            if (status || !info.size) break;
            // copy the meta data and buffer to be passed to decoder
            if (inputBufferOffset + info.size >= kMaxBufferSize) {
                LOGE("Memory allocated not sufficient");
                callbackInfo = "Memory allocated not sufficient \n";
                PostStatusMessage(callbackInfo.c_str());
                return;
            }

            memcpy(inputBuffer + inputBufferOffset, mHwExtractor->getFrameBuf(), info.size);
            frameInfo.push_back(info);
            inputBufferOffset += info.size;
        }

        outputFp = fopen(sOutPath2.c_str(), "wb");
        if (!outputFp) {
            LOGE("Unable to open :%s", sOutPath2.c_str());
            callbackInfo =
                    "Unable to open " + sOutPath2 + "\n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }


        string codecName = sCodecName;
        bool asyncMode = false;
        pHwDeCodec->setupDecoder();
        status = pHwDeCodec->decode(inputBuffer, frameInfo, codecName, asyncMode, outputFp);
        if (status != AMEDIA_OK) {
            LOGE("Decoder failed for %s", codecName.c_str());
            callbackInfo = "Decoder failed for" + codecName + " \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }

        AMediaFormat *decoderFormat = pHwDeCodec->getFormat();

        ifstream eleStream;
        eleStream.open(sOutPath2.c_str(), ifstream::binary | ifstream::ate);
        if (!eleStream.is_open()) {
            LOGE("%s file not found", sOutPath2.c_str());
            callbackInfo = "not found file:" + sOutPath2 + " \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        size_t eleSize = eleStream.tellg();
        eleStream.seekg(0, ifstream::beg);

        AMediaFormat *format = mHwExtractor->getFormat();
        const char *mime = nullptr;
        AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime);
        if (mime == nullptr) {
            LOGE("Invalid mime type");
            callbackInfo = "Invalid mime type  \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }

        // Get encoder params
        encParameter encParams;
        if (!strncmp(mime, "video/", 6)) {
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_WIDTH, &encParams.width);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_HEIGHT, &encParams.height);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_FRAME_RATE, &encParams.frameRate);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_BIT_RATE, &encParams.bitrate);
            if (encParams.bitrate <= 0 || encParams.frameRate <= 0) {
                encParams.frameRate = 25;
                if (!strcmp(mime, "video/3gpp") || !strcmp(mime, "video/mp4v-es")) {
                    encParams.bitrate = kEncodeMinVideoBitRate;
                } else {
                    encParams.bitrate = kEncodeDefaultVideoBitRate;
                }
            }
            AMediaFormat_getInt32(decoderFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT,
                                  &encParams.colorFormat);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_PROFILE, &encParams.profile);
//            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_LEVEL, &encParams.level);
            LOGI("Get encoder params: %dx%d, frameRate: %lld us,profile :%d,level:%d",
                 encParams.width, encParams.height, encParams.frameRate,
                 encParams.profile, encParams.level);
        } else {
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_SAMPLE_RATE,
                                  &encParams.sampleRate);
            AMediaFormat_getInt32(format, AMEDIAFORMAT_KEY_CHANNEL_COUNT,
                                  &encParams.numChannels);
            encParams.bitrate = kEncodeDefaultAudioBitRate;
        }

        pHwEnCodec->setupEncoder();
        LOGE("pHwEnCodec->setupEncoder()");

        string enCodecName = "";
        bool enCodecAsyncMode = false;
        status = pHwEnCodec->encode(enCodecName, eleStream, eleSize, enCodecAsyncMode, encParams,
                                    (char *) mime);
        if (status != AMEDIA_OK) {
            LOGE("Encoder failed for %s", enCodecName.c_str());
            callbackInfo = "Encoder failed for" + enCodecName + " \n";
            PostStatusMessage(callbackInfo.c_str());
            return;
        }
        pHwEnCodec->deInitCodec();
        LOGI("codec : %s", codecName.c_str());
        pHwEnCodec->dumpStatistics(sSrcPath, mHwExtractor->getClipDuration(), codecName,
                                   (asyncMode ? "async" : "sync"), sOutPath1);
        eleStream.close();

        pHwDeCodec->deInitCodec();
        LOGI("codec : %s", codecName.c_str());
        pHwDeCodec->dumpStatistics(sSrcPath, codecName, (asyncMode ? "async" : "sync"),
                                   sOutPath1);
        free(inputBuffer);
        pHwDeCodec->resetDecoder();
    }
    fclose(inputFp);
    fclose(outputFp);
    mHwExtractor->deInitExtractor();
    delete pHwDeCodec;

    LOGI("ProcessEnCodec Success");
    callbackInfo = "ProcessEnCodec Success outfile:" + sOutPath2 + " \n";
    PostStatusMessage(callbackInfo.c_str());

}


bool ProcessEnCodec::writeStatsHeader() {
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


JNIEnv *ProcessEnCodec::GetJNIEnv(bool *isAttach) {
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

void ProcessEnCodec::PostStatusMessage(const char *msg) {
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