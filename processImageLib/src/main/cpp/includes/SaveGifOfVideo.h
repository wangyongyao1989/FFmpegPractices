//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/4.
//

#ifndef FFMPEGPRACTICE_SAVEGIFOFVIDEO_H
#define FFMPEGPRACTICE_SAVEGIFOFVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

class SaveGifOfVideo {

private:

    string saveGifInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    string sSrcPath;
    string sDestPath;

    int save_index = 50;

    int packet_index = -1; // 数据包的索引序号

    char errbuf[1024];

    AVFormatContext *in_fmt_ctx = NULL; // 输入文件的封装器实例
    AVCodecContext *video_decode_ctx = NULL; // 视频解码器的实例
    int video_index = -1; // 视频流的索引
    AVFormatContext *gif_fmt_ctx = NULL; // GIF图片的封装器实例
    AVCodecContext *gif_encode_ctx = NULL; // GIF编码器的实例
    AVStream *src_video = NULL; // 源文件的视频流
    AVStream *gif_stream = NULL;  // GIF数据流
    enum AVPixelFormat target_format = AV_PIX_FMT_BGR8; // gif的像素格式
    struct SwsContext *swsContext = NULL; // 图像转换器的实例
    AVFrame *rgb_frame = NULL; // RGB数据帧

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processImageProcedure();

    int open_input_file(const char *srcPath);

    int open_output_file(const char *gif_name);

    int init_sws_context(void);

    int decode_video(AVPacket *packet, AVFrame *frame, int save_index);

    int save_gif_file(AVFrame *frame, int save_index);

public:

    SaveGifOfVideo(JNIEnv *env, jobject thiz);

    ~SaveGifOfVideo();

    void startWriteGif(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_SAVEGIFOFVIDEO_H
