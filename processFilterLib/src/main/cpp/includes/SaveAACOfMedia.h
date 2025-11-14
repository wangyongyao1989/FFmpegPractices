//
// Created by wangyao on 2025/9/6.
//

#ifndef FFMPEGPRACTICE_SAVEAACOFMEDIA_H
#define FFMPEGPRACTICE_SAVEAACOFMEDIA_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SaveAACOfMedia {

private:
    string saveAACInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sDestPath;

    char errbuf[1024];

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例
    AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
    int audio_index = -1; // 音频流的索引
    AVCodecContext *audio_encode_ctx = nullptr; // AAC编码器的实例


    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processAudioProcedure();

    int open_input_file(const char *src_name);

    int init_audio_encoder();

    int save_aac_file(FILE *fp_out, AVFrame *frame);

    void get_adts_header(AVCodecContext *codec_ctx, char *adts_header, int aac_length);


public:
    SaveAACOfMedia(JNIEnv *env, jobject thiz);

    ~SaveAACOfMedia();

    void startSaveAAC(const char *srcPath, const char *destPath);
};


#endif //FFMPEGPRACTICE_SAVEAACOFMEDIA_H
