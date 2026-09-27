//
// Created by wangyao on 2025/10/5.
//

#ifndef FFMPEGPRACTICE_MEDIAEXTRATORDECODECENCODEC_H
#define FFMPEGPRACTICE_MEDIAEXTRATORDECODECENCODEC_H

#include <jni.h>
#include <string>
#include <android/log.h>
#include "string"
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>
#include <thread>
#include <fstream>

#include "LogUtils.h"
#include "BenchmarkCommon.h"
#include "AndroidThreadManager.h"

using namespace std;

struct encodecParameter {
    int32_t bitrate = -1;
    int32_t numFrames = -1;
    int32_t frameSize = -1;
    int32_t sampleRate = 0;
    int32_t numChannels = 0;
    int32_t maxFrameSize = -1;
    int32_t width = 0;
    int32_t height = 0;
    int32_t frameRate = -1;
    int32_t iFrameInterval = 5;
    int32_t profile = -1;
    int32_t level = 0x100;
    int32_t colorFormat = 0x7F420888;
};

class MediaExtratorDecodecEncodec : CallBackHandle {
private:

    string callbackInfo;

    int32_t kEncodeDefaultVideoBitRate = 8000000 /* 8 Mbps */;
    int32_t kEncodeMinVideoBitRate = 600000 /* 600 Kbps */;
    int32_t kEncodeDefaultAudioBitRate = 128000 /* 128 Kbps */;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    AMediaExtractor *extractor = nullptr;

    AMediaCodec *mVideoDeCodec = nullptr;
    AMediaCodec *mAudioDeCodec = nullptr;

    AMediaCodec *mVideoEnCodec = nullptr;
    AMediaCodec *mAudioEnCodec = nullptr;

    AMediaFormat *mVideoFormat = nullptr;
    AMediaFormat *mAudioFormat = nullptr;

    int videoTrackIndex = -1;
    int audioTrackIndex = -1;

    const char *video_mime = nullptr;

    const char *audio_mime = nullptr;

    // 音频格式信息
    // 与 MediaExtratorDecodec 同样的问题：标量成员无初值，且对象被 JNI 层缓存复用，
    // EOS/错误标志和帧计数不会随新一轮点击复位，drain 循环因此可能一帧都不进。
    int audioSampleRate = 0;
    int audioChannelCount = 0;


    bool hasVideo = false;
    bool hasAudio = false;

    string sSrcPath;
    string sOutPath1;
    string sOutPath2;


    int32_t mNumOutputDecodecVideoFrame = 0;
    int32_t mNumOutputDecodecAudioFrame = 0;

    bool mSawInputDecodecEOS = false;
    bool mSawOutputDecodecEOS = false;
    bool mSignalledDecodecError = false;
    media_status_t mErrorCode = AMEDIA_OK;

    int32_t mOffset = 0;
    AMediaCodecBufferInfo mFrameMetaData;
    FILE *mDecodecOutFp = nullptr;
    FILE *mEncodecOutFp = nullptr;


    encodecParameter mEncParams;
    size_t mEncodecInputBufferSize = 0;

    bool mSawInputEncodecEOS = false;
    bool mSawOutputEncodecEOS = false;
    bool mSignalledEncodecError = false;

    int mNumInputFrame = 0;
    int mNumOutputVideoFrame = 0;
    ifstream *mEleStream = nullptr;


    /* Asynchronous locks */
    mutex mMutex;
    condition_variable mDecoderDoneCondition;

    std::unique_ptr<AndroidThreadManager> g_threadManager;

    bool isDeCodec = false;

    std::ifstream *mSrcEleStream = nullptr;

    // 输入文件的 FILE*。原先是 initExtractor() 里的局部变量，函数一返回就没人管了，
    // 每点一次按钮泄漏一个 fd；放到成员里由 release() 统一关。
    // 不直接在 initExtractor() 里 fclose，是因为 AMediaExtractor_setDataSourceFd
    // 对 fd 所有权的说明并没有写进 NDK 头文件，保守起见让它在整个解码期间保持打开。
    FILE *mInputFp = nullptr;


    bool initExtractor();

    bool selectTracksAndGetFormat();

    bool initDecodec(bool asyncMode);

    bool initEncodec(bool asyncMode);

    bool decodec();

    bool encodec(bool asyncMode);

    void release();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    // Async callback APIs
    void onInputAvailable(AMediaCodec *mediaCodec, int32_t index) override;

    void onDecodecInputAvailable(AMediaCodec *mediaDeCodec, int32_t index);

    void onEncodecInputAvailable(AMediaCodec *mediaEnCodec, int32_t index);

    void onFormatChanged(AMediaCodec *codec, AMediaFormat *format) override;

    void onError(AMediaCodec *mediaCodec, media_status_t err) override;

    void onOutputAvailable(AMediaCodec *codec, int32_t index,
                           AMediaCodecBufferInfo *bufferInfo) override;

    void onDecodecOutputAvailable(AMediaCodec *codec, int32_t index,
                                  AMediaCodecBufferInfo *bufferInfo);

    void onEncodecOutputAvailable(AMediaCodec *codec, int32_t index,
                                  AMediaCodecBufferInfo *bufferInfo);


public:
    MediaExtratorDecodecEncodec(JNIEnv *env, jobject thiz);

    ~MediaExtratorDecodecEncodec();

    void startMediaExtratorDecodecEncodec(const char *inputPath, const char *outpath1,
                                          const char *outpath2);
};

#endif //FFMPEGPRACTICE_MEDIAEXTRATORDECODECENCODEC_H
