//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/29.
//

#ifndef FFMPEGPRACTICE_MediaExtratorDecodec_H
#define FFMPEGPRACTICE_MediaExtratorDecodec_H

#include <jni.h>
#include <string>
#include <android/log.h>
#include "string"
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>
#include <thread>

#include "LogUtils.h"
#include "BenchmarkCommon.h"
#include "AndroidThreadManager.h"

using namespace std;

class MediaExtratorDecodec : CallBackHandle {
private:

    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    AMediaExtractor *extractor = nullptr;
    AMediaCodec *mVideoCodec = nullptr;
    AMediaCodec *mAudioCodec = nullptr;

    AMediaFormat *mVideoFormat = nullptr;
    AMediaFormat *mAudioFormat = nullptr;

    int videoTrackIndex = -1;
    int audioTrackIndex = -1;
    // 视频格式信息
    int videoWidth;
    int videoHeight;
    int64_t videoDuration;

    const char *video_mime = nullptr;

    const char *audio_mime = nullptr;

    // 音频格式信息
    int audioSampleRate;
    int audioChannelCount;


    bool hasVideo = false;
    bool hasAudio = false;

    string sSrcPath;

    int32_t mNumOutputVideoFrame;
    int32_t mNumOutputAudioFrame;

    bool mSawInputEOS;
    bool mSawOutputEOS;
    bool mSignalledError;
    media_status_t mErrorCode;

    int32_t mOffset;
    AMediaCodecBufferInfo mFrameMetaData;
    FILE *mOutFp;

    /* Asynchronous locks */
    mutex mMutex;
    condition_variable mDecoderDoneCondition;

    std::unique_ptr<AndroidThreadManager> g_threadManager;


    bool initExtractor();

    bool selectTracksAndGetFormat();

    bool initDecodec(bool asyncMode);

    bool decodec();

    void release();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    // Async callback APIs
    void onInputAvailable(AMediaCodec *codec, int32_t index) override;

    void onFormatChanged(AMediaCodec *codec, AMediaFormat *format) override;

    void onError(AMediaCodec *mediaCodec, media_status_t err) override;

    void onOutputAvailable(AMediaCodec *codec, int32_t index,
                           AMediaCodecBufferInfo *bufferInfo) override;


public:
    MediaExtratorDecodec(JNIEnv *env, jobject thiz);

    ~MediaExtratorDecodec();

    void startMediaExtratorDecodec(const char *inputPath);
};




#endif //FFMPEGPRACTICE_MediaExtratorDecodec_H
