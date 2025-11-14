//
// Created by wangyao on 2025/9/6.
//

#ifndef FFMPEGPRACTICE_SAVEWAVOFMEDIA_H
#define FFMPEGPRACTICE_SAVEWAVOFMEDIA_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

typedef struct WAVHeader {
    char riffCkID[4]; // 固定填"RIFF"
    int32_t riffCkSize; // RIFF块大小。文件总长减去riffCkID和riffCkSize两个字段的长度
    char format[4]; // 固定填"WAVE"
    char fmtCkID[4]; // 固定填"fmt "
    int32_t fmtCkSize; // 格式块大小，从audioFormat到bitsPerSample各字段长度之和，为16
    int16_t audioFormat; // 音频格式。1表示整数，3表示浮点数
    int16_t channels; // 声道数量
    int32_t sampleRate; // 采样频率，单位赫兹
    int32_t byteRate; // 数据传输速率，单位字节每秒
    int16_t blockAlign; // 采样大小，即每个样本占用的字节数
    int16_t bitsPerSample; // 每个样本占用的比特数量，即采样大小乘以8（样本大小以字节为单位）
    char dataCkID[4]; // 固定填"data"
    int32_t dataCkSize; // 数据块大小。文件总长减去WAV头的长度
} WAVHeader;

class SaveWavOfMedia {

private:
    string saveWavInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath1;
    string sDestPath1;
    string sDestPath2;

    char errbuf[1024];

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例
    AVCodecContext *audio_decode_ctx = nullptr; // 音频解码器的实例
    int audio_index = -1; // 音频流的索引
    AVCodecContext *audio_encode_ctx = nullptr; // AAC编码器的实例


    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processAudioProcedure();

    int save_wav_file(const char *pcm_name);


public:
    SaveWavOfMedia(JNIEnv *env, jobject thiz);

    ~SaveWavOfMedia();

    void startSaveWav(const char *srcPath1, const char *destPath1, const char *destPath2);

};


#endif //FFMPEGPRACTICE_SAVEWAVOFMEDIA_H
