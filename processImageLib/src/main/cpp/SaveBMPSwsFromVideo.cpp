//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/4.
//

#include "SaveBMPSwsFromVideo.h"

SaveBMPSwsFromVideo::SaveBMPSwsFromVideo(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveBMPSwsFromVideo::~SaveBMPSwsFromVideo() {
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

    if (bmp_fmt_ctx->pb) {
        avio_close(bmp_fmt_ctx->pb); // 关闭输出流
    }

    if (bmp_encode_ctx) {
        avcodec_close(bmp_encode_ctx); // 关闭视频编码器的实例
    }

    if (bmp_encode_ctx) {
        avcodec_free_context(&bmp_encode_ctx); // 释放视频编码器的实例
    }

    if (bmp_fmt_ctx) {
        avformat_free_context(bmp_fmt_ctx); // 释放封装器的实例
    }

}

void SaveBMPSwsFromVideo::startWriteBMPSws(const char *srcPath, const char *destPath) {
    sSrcPath = srcPath;
    sDestPath = destPath;
    processImageProcedure();
}


void SaveBMPSwsFromVideo::processImageProcedure() {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, sSrcPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", sSrcPath.c_str());
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveBMPInfo = "Can't open file:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveBMPInfo.c_str());
        return;
    }
    LOGI("Success open input_file %s.\n", sSrcPath.c_str());
    saveBMPInfo = "Success open input_file:" + sSrcPath;
    PostStatusMessage(saveBMPInfo.c_str());

    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveBMPInfo = "Can't find stream information.:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveBMPInfo.c_str());
        return;
    }
    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        AVStream *src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            saveBMPInfo = "video_codec not found \n";
            PostStatusMessage(saveBMPInfo.c_str());
            return;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            saveBMPInfo = "video_decode_ctx is null \n";
            PostStatusMessage(saveBMPInfo.c_str());
            return;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            saveBMPInfo = "Can't open video_decode_ctx. \n";
            PostStatusMessage(saveBMPInfo.c_str());
            return;
        }
    } else {
        LOGE("Can't find video stream.\n");
        saveBMPInfo = "Can't find video stream.. \n";
        PostStatusMessage(saveBMPInfo.c_str());
        return;
    }

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            ret = decode_video(packet, frame, save_index); // 对视频帧解码
            if (ret == 0) {
                break; // 只保存一幅图像就退出
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    LOGI("Success save %d_index frame as png file.\n", save_index);
    saveBMPInfo = "Success save " + to_string(save_index) + "_index frame as yuv file.\n";
    PostStatusMessage(saveBMPInfo.c_str());

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}


// 对视频帧解码。save_index表示要把第几个视频帧保存为图片
int SaveBMPSwsFromVideo::decode_video(AVPacket *packet, AVFrame *frame, int save_index) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveBMPInfo = "send packet occur error:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveBMPInfo.c_str());
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE("decode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveBMPInfo = "decode frame occur error :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveBMPInfo.c_str());
            break;
        }
        packet_index++;
        if (packet_index < save_index) { // 还没找到对应序号的帧
            return AVERROR(EAGAIN);
        }
        save_bmp_file(frame, save_index); // 把视频帧保存为JPEG图片
        break;
    }
    return ret;
}

// 把视频帧保存为BMP图片。save_index表示要把第几个视频帧保存为图片
int SaveBMPSwsFromVideo::save_bmp_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    LOGI("format = %d, width = %d, height = %d\n",
         frame->format, frame->width, frame->height);
    saveBMPInfo = "format =" + to_string(frame->format) + ",width =" + to_string(frame->width) +
                  ",height =" + to_string(frame->width) + "\n";
    saveBMPInfo = saveBMPInfo + "target image file is:" + sDestPath.c_str() + "\n";
    PostStatusMessage(saveBMPInfo.c_str());

    FILE *fp = fopen(sDestPath.c_str(), "wb"); // 以写方式打开文件
    if (!fp) {
        LOGE("open file %s fail.\n", sDestPath.c_str());
        saveBMPInfo = "open file " + sDestPath + " fail.";
        PostStatusMessage(saveBMPInfo.c_str());
        return -1;
    }

    enum AVPixelFormat target_format = AV_PIX_FMT_BGR24; // // bmp的像素格式是BGR24
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    struct SwsContext *swsContext = sws_getContext(
            frame->width, frame->height, AV_PIX_FMT_YUV420P,
            frame->width, frame->height, target_format,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext == nullptr) {
        LOGE("swsContext is null\n");
        saveBMPInfo = "swsContext is null \n";
        PostStatusMessage(saveBMPInfo.c_str());
        return -1;
    }

    // 分配缓冲区空间，用于存放转换后的图像数据
    int buffer_size = av_image_get_buffer_size(target_format, frame->width, frame->height, 1);
    unsigned char *out_buffer = (unsigned char *) av_malloc(
            (size_t) buffer_size * sizeof(unsigned char));
    int linesize[4] = {3 * frame->width, 0, 0, 0};
    // 转换器开始处理图像数据，把YUV图像转为RGB图像
    sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize,
              0, frame->height, (uint8_t **) &out_buffer, linesize);
    sws_freeContext(swsContext); // 释放图像转换器的实例

    BITMAPFILEHEADER bmp_header; // 声明bmp文件的头结构
    BITMAPINFOHEADER bmp_info; // 声明bmp文件的信息结构
    unsigned int data_size = (frame->width * 3 + 3) / 4 * 4 * frame->height;
    // 文件标识填“BM”（即0x4D42）表示位图
    bmp_header.bfType = 0x4D42;
    // 保留字段1。填0即可
    bmp_header.bfReserved1 = 0;
    // 保留字段2。填0即可
    bmp_header.bfReserved2 = 0;
    // 从文件开头到位图数据的偏移量（单位字节）
    bmp_header.bfOffBits = sizeof(bmp_header) + sizeof(bmp_info);
    // 整个文件的大小（单位字节）
    bmp_header.bfSize = bmp_header.bfOffBits + data_size;
    // 信息头的长度（单位字节）
    bmp_info.biSize = sizeof(bmp_info);
    // 位图宽度（单位像素）
    bmp_info.biWidth = frame->width;
    // 位图高度（单位像素）。若为正，表示倒向的位图；若为负，表示正向的位图
    bmp_info.biHeight = frame->height;
    // 位图的面数。填1即可
    bmp_info.biPlanes = 1;
    // 单个像素的位数（单位比特）。RGB各一个字节，总共3个字节也就是24位
    bmp_info.biBitCount = 24;
    // 压缩说明。0(BI_RGB)表示不压缩
    bmp_info.biCompression = 0;
    // 位图数据的大小（单位字节）
    bmp_info.biSizeImage = data_size;
    // 水平打印分辨率（单位：像素/米）。填0即可
    bmp_info.biXPelsPerMeter = 0;
    // 垂直打印分辨率（单位：像素/米）。填0即可
    bmp_info.biYPelsPerMeter = 0;
    // 位图使用的颜色掩码。填0即可
    bmp_info.biClrUsed = 0;
    // 重要的颜色个数。都是普通颜色，填0即可
    bmp_info.biClrImportant = 0;
    fwrite(&bmp_header, sizeof(bmp_header), 1, fp); // 写入bmp文件头
    fwrite(&bmp_info, sizeof(bmp_info), 1, fp); // 写入bmp信息头
    uint8_t tmp[frame->width * 3]; // 临时数据
    for (int i = 0; i < frame->height / 2; i++) { // 把缓冲区的图像数据倒置过来
        memcpy(tmp, &(out_buffer[frame->width * i * 3]), frame->width * 3);
        memcpy(&(out_buffer[frame->width * i * 3]),
               &(out_buffer[frame->width * (frame->height - 1 - i) * 3]), frame->width * 3);
        memcpy(&(out_buffer[frame->width * (frame->height - 1 - i) * 3]), tmp, frame->width * 3);
    }
    fwrite(out_buffer, frame->width * frame->height * 3, 1, fp); // 写入图像数据
    fclose(fp); // 关闭文件
    return 0;
}


JNIEnv *SaveBMPSwsFromVideo::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("SaveYUVFromVideo::GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("SaveYUVFromVideo::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void SaveBMPSwsFromVideo::PostStatusMessage(const char *msg) {
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