//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/4.
//

#ifndef FFMPEGPRACTICE_SAVEBMPSWSFROMVIDEO_H
#define FFMPEGPRACTICE_SAVEBMPSWSFROMVIDEO_H

#include <jni.h>
#include <thread>
#include "BasicCommon.h"
#include "string"

using namespace std;

// 把内存对齐定义为2个字节，可避免因BITMAPFILEHEADER出现4字节的对齐造成bmp位图头出错的问题。
// 很重要，如果不设置就会导致bmp数据格式错误！！！
#pragma pack(2)

// 定义位图文件头的结构
typedef struct BITMAPFILEHEADER {
    uint16_t bfType; // 文件类型
    uint32_t bfSize; // 文件大小
    uint16_t bfReserved1; // 保留字段1
    uint16_t bfReserved2; // 保留字段2
    uint32_t bfOffBits; // 从文件开头到位图数据的偏移量（单位字节）
} BITMAPFILEHEADER;

// 定义位图信息头的结构
typedef struct BITMAPINFOHEADER {
    uint32_t biSize; // 信息头的长度（单位字节）
    uint32_t biWidth; // 位图宽度（单位像素）
    uint32_t biHeight; // 位图高度（单位像素）
    uint16_t biPlanes; // 位图的面数（单位像素）
    uint16_t biBitCount; // 单个像素的位数（单位比特）
    uint32_t biCompression; // 压缩说明
    uint32_t biSizeImage; // 位图数据的大小（单位字节）
    uint32_t biXPelsPerMeter; // 水平打印分辨率
    uint32_t biYPelsPerMeter; // 垂直打印分辨率
    uint32_t biClrUsed; // 位图使用的颜色掩码
    uint32_t biClrImportant; // 重要的颜色个数
} BITMAPINFOHEADER;

class SaveBMPSwsFromVideo {
private:
    string saveBMPInfo;

    JavaVM *mJavaVm = nullptr;
    jobject mJavaObj = nullptr;
    JNIEnv *mEnv = nullptr;

    thread *mThread = nullptr;

    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例

    AVCodecContext *video_decode_ctx = nullptr;
    AVCodecContext *bmp_encode_ctx = nullptr;

    AVFormatContext *bmp_fmt_ctx = nullptr;

    string sSrcPath;
    string sDestPath;

    int save_index = 0;

    int packet_index = -1; // 数据包的索引序号

    char errbuf[1024];

    JNIEnv *GetJNIEnv(bool *isAttach);

    void PostStatusMessage(const char *msg);

    void processImageProcedure();

    int decode_video(AVPacket *packet, AVFrame *frame, int save_index);

    int save_bmp_file(AVFrame *frame, int save_index);

public:
    SaveBMPSwsFromVideo(JNIEnv *env, jobject thiz);

    ~SaveBMPSwsFromVideo();

    void startWriteBMPSws(const char *srcPath, const char *destPath);
};


#endif //FFMPEGPRACTICE_SAVEBMPSWSFROMVIDEO_H
