//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/29.
//

#include <sys/stat.h>
#include "includes/MediaExtratorDecodec.h"


MediaExtratorDecodec::MediaExtratorDecodec(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

MediaExtratorDecodec::~MediaExtratorDecodec() {
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

    release();

}

void MediaExtratorDecodec::startMediaExtratorDecodec(const char *inputPath, const char *outputPath) {
    sSrcPath = inputPath;
    sOutPath = outputPath;

    LOGI("sSrcPath :%s \n sOutPath: %s ", sSrcPath.c_str(), sOutPath.c_str());
    callbackInfo =
            "sSrcPath:" + sSrcPath + "\n";
    PostStatusMessage(callbackInfo.c_str());
    // 1. 初始化提取器
    if (!initExtractor()) {
        LOGE("Failed to initialize extractor");
        callbackInfo =
                "Failed to initialize extractor \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 2. 选择轨道
    if (!selectTracks()) {
        LOGE("No valid tracks found");
        callbackInfo =
                "No valid tracks found \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 3. 初始化复用器
    if (!initMuxer()) {
        LOGE("Failed to initialize muxer");
        callbackInfo =
                "Failed to initialize muxer \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 4. 执行转封装
    if (!transmux()) {
        LOGE("Transmuxing failed");
        callbackInfo =
                "Transmuxing failed \n";
        PostStatusMessage(callbackInfo.c_str());
        return;
    }

    // 释放资源
    release();
}

// 初始化提取器
bool MediaExtratorDecodec::initExtractor() {

    extractor = AMediaExtractor_new();
    if (!extractor) {
        LOGE("Failed to create media extractor ");
        callbackInfo =
                "Failed to create media extractor \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    LOGE("inputPath:%c", sSrcPath.c_str());
    FILE *inputFp = fopen(sSrcPath.c_str(), "rb");
    if (!inputFp) {
        LOGE("Unable to open output file :%s", sSrcPath.c_str());
        callbackInfo =
                "Unable to open output file :" + sSrcPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    struct stat buf;
    stat(sSrcPath.c_str(), &buf);
    size_t fileSize = buf.st_size;
    int32_t input_fd = fileno(inputFp);

    LOGE("input_fd:%d", input_fd);
    media_status_t status = AMediaExtractor_setDataSourceFd(extractor, input_fd, 0, fileSize);
    if (status != AMEDIA_OK) {
        LOGE("Failed to set data source: %d", status);
        callbackInfo =
                "Failed to set data source :" + to_string(status) + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    LOGI("Extractor initialized successfully");
    return true;
}

// 选择轨道
bool MediaExtratorDecodec::selectTracks() {
    size_t trackCount = AMediaExtractor_getTrackCount(extractor);
    LOGI("Total tracks: %zu", trackCount);
    callbackInfo =
            "Total tracks:" + to_string(trackCount) + "\n";
    PostStatusMessage(callbackInfo.c_str());

    for (size_t i = 0; i < trackCount; i++) {
        AMediaFormat *format = AMediaExtractor_getTrackFormat(extractor, i);
        if (!format) continue;

        const char *mime;
        if (AMediaFormat_getString(format, AMEDIAFORMAT_KEY_MIME, &mime)) {
            LOGI("Track %zu: MIME=%s", i, mime);

            if (strncmp(mime, "video/", 6) == 0 && videoTrackIndex == -1) {
                videoTrackIndex = i;
                hasVideo = true;
                LOGI("Selected video track: %d", videoTrackIndex);
                callbackInfo =
                        "Selected video track:" + to_string(videoTrackIndex) + "\n";
                PostStatusMessage(callbackInfo.c_str());
            } else if (strncmp(mime, "audio/", 6) == 0 && audioTrackIndex == -1) {
                audioTrackIndex = i;
                hasAudio = true;
                LOGI("Selected audio track: %d", audioTrackIndex);
                callbackInfo =
                        "Selected audio track:" + to_string(audioTrackIndex) + "\n";
                PostStatusMessage(callbackInfo.c_str());
            }
        }

        AMediaFormat_delete(format);
    }

    return hasVideo || hasAudio;
}

// 初始化复用器
bool MediaExtratorDecodec::initMuxer() {
    LOGE("outputPath:%c", sOutPath.c_str());
    FILE *outputFp = fopen(sOutPath.c_str(), "w+b");
    if (!outputFp) {
        LOGE("Unable to open output file :%s", sOutPath.c_str());
        callbackInfo =
                "Unable to open output file :" + sOutPath + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }
    int32_t output_fd = fileno(outputFp);

    muxer = AMediaMuxer_new(output_fd, AMEDIAMUXER_OUTPUT_FORMAT_MPEG_4);
    if (!muxer) {
        LOGE("Failed to create media muxer");
        callbackInfo =
                "Failed to create media muxer \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    // 添加视频轨道
    if (hasVideo) {
        AMediaExtractor_selectTrack(extractor, videoTrackIndex);
        AMediaFormat *videoFormat = AMediaExtractor_getTrackFormat(extractor, videoTrackIndex);
        muxerVideoTrackIndex = AMediaMuxer_addTrack(muxer, videoFormat);
        AMediaFormat_delete(videoFormat);

        if (muxerVideoTrackIndex < 0) {
            LOGE("Failed to add video track to muxer");
            callbackInfo =
                    "Failed to add video track to muxer \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }
        LOGI("Muxer video track index: %d", muxerVideoTrackIndex);
        callbackInfo =
                "Muxer video track index:" + to_string(muxerVideoTrackIndex) + "\n";
        PostStatusMessage(callbackInfo.c_str());
    }

    // 添加音频轨道
    if (hasAudio) {
        AMediaExtractor_selectTrack(extractor, audioTrackIndex);
        AMediaFormat *audioFormat = AMediaExtractor_getTrackFormat(extractor, audioTrackIndex);
        muxerAudioTrackIndex = AMediaMuxer_addTrack(muxer, audioFormat);
        AMediaFormat_delete(audioFormat);

        if (muxerAudioTrackIndex < 0) {
            LOGE("Failed to add audio track to muxer");
            callbackInfo =
                    "Failed to add audio track to muxer \n";
            PostStatusMessage(callbackInfo.c_str());
            return false;
        }
        LOGI("Muxer audio track index: %d", muxerAudioTrackIndex);
        callbackInfo =
                "Muxer audio track index:" + to_string(muxerAudioTrackIndex) + "\n";
        PostStatusMessage(callbackInfo.c_str());
    }

    // 启动复用器
    media_status_t status = AMediaMuxer_start(muxer);
    if (status != AMEDIA_OK) {
        LOGE("Failed to start muxer: %d", status);
        callbackInfo =
                "Failed to start muxer:" + to_string(status) + "\n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    LOGI("Muxer initialized successfully");
    callbackInfo =
            "Muxer initialized successfully \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}


// 执行转封装
bool MediaExtratorDecodec::transmux() {
    if (!extractor || !muxer) {
        LOGE("Extractor or muxer not initialized");
        callbackInfo =
                "Extractor or muxer not initialized \n";
        PostStatusMessage(callbackInfo.c_str());
        return false;
    }

    AMediaCodecBufferInfo info;
    bool sawEOS = false;
    int64_t lastVideoPts = -1;
    int64_t lastAudioPts = -1;

    // 重新选择所有轨道以重置读取位置
    if (hasVideo) AMediaExtractor_selectTrack(extractor, videoTrackIndex);
    if (hasAudio) AMediaExtractor_selectTrack(extractor, audioTrackIndex);

    // 设置读取位置到开始
    AMediaExtractor_seekTo(extractor, 0, AMEDIAEXTRACTOR_SEEK_CLOSEST_SYNC);

    while (!sawEOS) {
        ssize_t trackIndex = AMediaExtractor_getSampleTrackIndex(extractor);

        if (trackIndex < 0) {
            sawEOS = true;
            break;
        }

        // 获取样本信息
        info.size = AMediaExtractor_getSampleSize(extractor);
        info.offset = 0;
        info.flags = AMediaExtractor_getSampleFlags(extractor);
        info.presentationTimeUs = AMediaExtractor_getSampleTime(extractor);

        // 读取样本数据
        uint8_t *sampleData = new uint8_t[info.size];
        ssize_t bytesRead = AMediaExtractor_readSampleData(extractor, sampleData, info.size);

        if (bytesRead < 0) {
            LOGE("Error reading sample data: %zd", bytesRead);
            callbackInfo =
                    "Error reading sample data:" + to_string(bytesRead) + "\n";
            PostStatusMessage(callbackInfo.c_str());
            delete[] sampleData;
            break;
        }

        // 写入到复用器
        if (trackIndex == videoTrackIndex && hasVideo) {
            // 检查时间戳是否有效（避免重复或倒退的时间戳）
            if (info.presentationTimeUs > lastVideoPts) {
                media_status_t status = AMediaMuxer_writeSampleData(muxer,
                                                                    muxerVideoTrackIndex,
                                                                    sampleData,
                                                                    &info);
                if (status != AMEDIA_OK) {
                    LOGE("Failed to write video sample: %d", status);
                    callbackInfo =
                            "Failed to write video sample:" + to_string(status) + "\n";
                    PostStatusMessage(callbackInfo.c_str());
                }
                callbackInfo =
                        "AMediaMuxer_writeSampleData video size:" + to_string(info.size) + "\n";
                PostStatusMessage(callbackInfo.c_str());
                lastVideoPts = info.presentationTimeUs;
            }
        } else if (trackIndex == audioTrackIndex && hasAudio) {
            // 检查时间戳是否有效
            if (info.presentationTimeUs > lastAudioPts) {
                media_status_t status = AMediaMuxer_writeSampleData(muxer,
                                                                    muxerAudioTrackIndex,
                                                                    sampleData,
                                                                    &info);
                if (status != AMEDIA_OK) {
                    LOGE("Failed to write audio sample : %d", status);
                    callbackInfo =
                            "Failed to write audio sample:" + to_string(status) + "\n";
                    PostStatusMessage(callbackInfo.c_str());
                }
                callbackInfo =
                        "AMediaMuxer_writeSampleData audio size:" + to_string(info.size) + "\n";
                PostStatusMessage(callbackInfo.c_str());
                lastAudioPts = info.presentationTimeUs;
            }
        }

        delete[] sampleData;

        // 前进到下一个样本
        if (!AMediaExtractor_advance(extractor)) {
            sawEOS = true;
        }
    }

    LOGI("Transmuxing completed");
    callbackInfo =
            "Transmuxing completed \n";
    PostStatusMessage(callbackInfo.c_str());
    return true;
}


// 释放资源
void MediaExtratorDecodec::release() {
    if (muxer) {
        AMediaMuxer_stop(muxer);
        AMediaMuxer_delete(muxer);
        muxer = nullptr;
    }

    if (extractor) {
        AMediaExtractor_delete(extractor);
        extractor = nullptr;
    }
    LOGI("Resources released");
}


JNIEnv *MediaExtratorDecodec::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void MediaExtratorDecodec::PostStatusMessage(const char *msg) {
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


