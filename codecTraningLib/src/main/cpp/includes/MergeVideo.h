//
// Created by 王耀 on 2025/8/30.
//

#ifndef FFMPEGPRACTICE_MERGEVIDEO_H
#define FFMPEGPRACTICE_MERGEVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;


class MergeVideo {

private:
    string mergeInfo;

    AVFormatContext *in_fmt_ctx[2] = {NULL, NULL}; // 输入文件的封装器实例
    AVCodecContext *video_decode_ctx[2] = {NULL, NULL}; // 视频解码器的实例
    int video_index[2] = {-1 - 1}; // 视频流的索引
    int audio_index[2] = {-1 - 1}; // 音频流的索引
    AVStream *src_video[2] = {NULL, NULL}; // 源文件的视频流
    AVStream *dest_video = NULL; // 目标文件的视频流
    AVFormatContext *out_fmt_ctx; // 输出文件的封装器实例
    AVCodecContext *video_encode_ctx = NULL; // 视频编码器的实例

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    thread *codecThread = nullptr;

    const char *mSrcPath1;
    const char *mSrcPath2;
    const char *mDestPath;

    char errbuf[1024];

    int count = 0;

    int open_input_file(int seq, const char *src_name);

    int open_output_file(const char *dest_name);

    int output_video(AVFrame *frame);

    int recode_video(int seq, AVPacket *packet, AVFrame *frame, int64_t begin_video_pts);

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void mergeVideo();

    static void DoCodecMedia(MergeVideo *recodecVideo);


public:
    MergeVideo(JNIEnv *env, jobject thiz);

    ~MergeVideo();

    void startMergeVideoThread(const char *srcPath1, const char *srcPath2, const char *destPath);
};


#endif //FFMPEGPRACTICE_MERGEVIDEO_H
