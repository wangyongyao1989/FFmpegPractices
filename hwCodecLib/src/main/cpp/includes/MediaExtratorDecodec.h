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
    // 本类没有自定义构造函数，下面这批标量成员初值不确定；而 JNI 层把对象
    // 缓存在全局指针里（HwCodecJniCall.cpp 的 if (mMediaExtratorDecodec == nullptr)），
    // 每点一次按钮都是同一个对象第二次进入 decodec()，于是这一批「一轮一清」的
    // 状态会带着上一轮的值：mSawOutputEOS 还是 true，drain 循环
    //     while (!mSawOutputEOS && !mSignalledError)
    // 一次都不进，输出文件 0 字节。所以这里全部给默认值，并在 decodec() 开头复位。
    int videoWidth = 0;
    int videoHeight = 0;
    int64_t videoDuration = 0;

    const char *video_mime = nullptr;

    const char *audio_mime = nullptr;

    // 音频格式信息
    int audioSampleRate = 0;
    int audioChannelCount = 0;


    bool hasVideo = false;
    bool hasAudio = false;

    string sSrcPath;
    string sOutPath;



    int32_t mNumOutputVideoFrame = 0;
    int32_t mNumOutputAudioFrame = 0;

    bool mSawInputEOS = false;
    bool mSawOutputEOS = false;
    bool mSignalledError = false;
    media_status_t mErrorCode = AMEDIA_OK;

    int32_t mOffset = 0;
    AMediaCodecBufferInfo mFrameMetaData;
    FILE *mOutFp = nullptr;

    /* Asynchronous locks */
    mutex mMutex;
    condition_variable mDecoderDoneCondition;

    std::unique_ptr<AndroidThreadManager> g_threadManager;

    // 输入文件的 FILE*。原先是 initExtractor() 里的局部变量，函数一返回就没人管了，
    // 每点一次按钮泄漏一个 fd；放到成员里由 release() 统一关。
    FILE *mInputFp = nullptr;


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

    void startMediaExtratorDecodec(const char *inputPath,const char *outpath);
};




#endif //FFMPEGPRACTICE_MediaExtratorDecodec_H
