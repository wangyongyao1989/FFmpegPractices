//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/29.
//

#ifndef FFMPEGPRACTICE_MEDIATRANSMUXER_H
#define FFMPEGPRACTICE_MEDIATRANSMUXER_H

#include <jni.h>
#include <string>
#include <android/log.h>
#include "string"
#include <media/NdkMediaExtractor.h>
#include <media/NdkMediaMuxer.h>
#include <media/NdkMediaFormat.h>
#include "LogUtils.h"

using namespace std;

class MediaTransMuxer {
private:

    string callbackInfo;


    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    AMediaExtractor *extractor = nullptr;
    AMediaMuxer *muxer = nullptr;

    int videoTrackIndex = -1;
    int audioTrackIndex = -1;
    int muxerVideoTrackIndex = -1;
    int muxerAudioTrackIndex = -1;

    bool hasVideo = false;
    bool hasAudio = false;

    // 输入/输出文件句柄。原先是 initExtractor()/initMuxer() 里的局部变量，
    // 两个 FILE* 全项目没有任何一处 fclose：每点一次按钮泄漏两个 fd。
    // 提成成员后由 release() 统一关（不提前关，是因为 AMediaMuxer/AMediaExtractor
    // 对传入 fd 的所有权 NDK 头文件里没写清楚，保持与改动前一致的存活区间最稳妥）。
    FILE *mInputFp = nullptr;
    FILE *mOutputFp = nullptr;

    string sSrcPath;
    string sOutPath;

    bool initExtractor();

    bool selectTracks();

    bool initMuxer();

    bool transmux();

    void release();

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

public:
    MediaTransMuxer(JNIEnv *env, jobject thiz);

    ~MediaTransMuxer();

    void startMediaTransMuxer(const char *inputPath, const char *outputPath);
};


#endif //FFMPEGPRACTICE_MEDIATRANSMUXER_H
