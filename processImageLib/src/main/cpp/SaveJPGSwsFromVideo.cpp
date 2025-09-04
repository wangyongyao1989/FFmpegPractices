//
// Created by wangyao on 2025/9/3.
//

#include "includes/SaveJPGSwsFromVideo.h"

SaveJPGSwsFromVideo::SaveJPGSwsFromVideo(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveJPGSwsFromVideo::~SaveJPGSwsFromVideo() {
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

    if (jpg_fmt_ctx->pb) {
        avio_close(jpg_fmt_ctx->pb); // 关闭输出流
    }

    if (jpg_encode_ctx) {
        avcodec_close(jpg_encode_ctx); // 关闭视频编码器的实例
    }

    if (jpg_encode_ctx) {
        avcodec_free_context(&jpg_encode_ctx); // 释放视频编码器的实例
    }

    if (jpg_fmt_ctx) {
        avformat_free_context(jpg_fmt_ctx); // 释放封装器的实例
    }


}

void SaveJPGSwsFromVideo::startWriteJPGSws(const char *srcPath, const char *destPath) {
    sSrcPath = srcPath;
    sDestPath = destPath;
    processImageProcedure();
}


void SaveJPGSwsFromVideo::processImageProcedure() {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, sSrcPath.c_str(), nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", sSrcPath.c_str());
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveJPGInfo = "Can't open file:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
        return;
    }
    LOGI("Success open input_file %s.\n", sSrcPath.c_str());
    saveJPGInfo = "Success open input_file:" + sSrcPath;
    PostStatusMessage(saveJPGInfo.c_str());

    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveJPGInfo = "Can't find stream information.:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
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
            saveJPGInfo = "video_codec not found \n";
            PostStatusMessage(saveJPGInfo.c_str());
            return;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            saveJPGInfo = "video_decode_ctx is null \n";
            PostStatusMessage(saveJPGInfo.c_str());
            return;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            saveJPGInfo = "Can't open video_decode_ctx. \n";
            PostStatusMessage(saveJPGInfo.c_str());
            return;
        }
    } else {
        LOGE("Can't find video stream.\n");
        saveJPGInfo = "Can't find video stream.. \n";
        PostStatusMessage(saveJPGInfo.c_str());
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
    LOGI("Success save %d_index frame as jpg file.\n", save_index);
    saveJPGInfo = "Success save " + to_string(save_index) + "_index frame as yuv file.\n";
    PostStatusMessage(saveJPGInfo.c_str());

    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}


// 对视频帧解码。save_index表示要把第几个视频帧保存为图片
int SaveJPGSwsFromVideo::decode_video(AVPacket *packet, AVFrame *frame, int save_index) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveJPGInfo = "send packet occur error:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
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
            saveJPGInfo = "decode frame occur error :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveJPGInfo.c_str());
            break;
        }
        packet_index++;
        if (packet_index < save_index) { // 还没找到对应序号的帧
            return AVERROR(EAGAIN);
        }
        save_jpg_file(frame, save_index); // 把视频帧保存为JPEG图片
        break;
    }
    return ret;
}

// 把视频帧保存为JPEG图片。save_index表示要把第几个视频帧保存为图片
int SaveJPGSwsFromVideo::save_jpg_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    LOGI("format = %d, width = %d, height = %d\n",
         frame->format, frame->width, frame->height);
    saveJPGInfo = "format =" + to_string(frame->format) + ",width =" + to_string(frame->width) +
                  ",height =" + to_string(frame->width) + "\n";
    saveJPGInfo = saveJPGInfo + "target image file is:" + sDestPath.c_str() + "\n";
    PostStatusMessage(saveJPGInfo.c_str());

    enum AVPixelFormat target_format = AV_PIX_FMT_YUVJ420P; // jpg的像素格式是YUVJ420P
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    struct SwsContext *swsContext = sws_getContext(
            frame->width, frame->height, AV_PIX_FMT_YUV420P,
            frame->width, frame->height, target_format,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext == nullptr) {
        LOGE("swsContext is null\n");
        saveJPGInfo = "swsContext is null \n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }
    AVFrame *yuvj_frame = av_frame_alloc(); // 分配一个YUVJ数据帧
    yuvj_frame->format = target_format; // 像素格式
    yuvj_frame->width = frame->width; // 视频宽度
    yuvj_frame->height = frame->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(yuvj_frame->data, yuvj_frame->linesize,
                   frame->width, frame->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, frame->width, frame->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(yuvj_frame->data, yuvj_frame->linesize, out_buffer,
//                       target_format, frame->width, frame->height, 1);
    // 转换器开始处理图像数据，把YUV图像转为YUVJ图像
    sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize,
              0, frame->height, yuvj_frame->data, yuvj_frame->linesize);
    sws_freeContext(swsContext); // 释放图像转换器的实例

    AVFormatContext *jpg_fmt_ctx = nullptr;
    // 分配JPEG文件的封装实例
    int ret = avformat_alloc_output_context2(&jpg_fmt_ctx, nullptr, nullptr, sDestPath.c_str());
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", sDestPath.c_str());
        saveJPGInfo = "Can't alloc output_file : " + sDestPath + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }
    // 查找MJPEG编码器
    AVCodec *jpg_codec = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_MJPEG);
    if (!jpg_codec) {
        LOGE("jpg_codec not found\n");
        saveJPGInfo = "jpg_codec not found \n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }
    // 获取编解码器上下文信息
    AVCodecContext *jpg_encode_ctx = avcodec_alloc_context3(jpg_codec);
    if (!jpg_encode_ctx) {
        LOGE("png_encode_ctx is null\n");
        saveJPGInfo = "png_encode_ctx is null \n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }
    // jpg的像素格式是YUVJ，对于MJPEG编码器来说，它支持YUVJ420P/YUVJ422P/YUVJ444P等格式
    jpg_encode_ctx->pix_fmt = target_format; // 像素格式
    jpg_encode_ctx->width = frame->width; // 视频宽度
    jpg_encode_ctx->height = frame->height; // 视频高度
    jpg_encode_ctx->time_base = (AVRational) {1, 25}; // 时间基
    LOGI("jpg codec_id = %d\n", jpg_encode_ctx->codec_id);
    ret = avcodec_open2(jpg_encode_ctx, jpg_codec, nullptr); // 打开编码器的实例
    if (ret < 0) {
        LOGE("Can't open jpg_encode_ctx.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveJPGInfo = "Can't open jpg_encode_ctx. :" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }
    AVStream *jpg_stream = avformat_new_stream(jpg_fmt_ctx, 0); // 创建数据流
    ret = avformat_write_header(jpg_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveJPGInfo = "write file_header occur error :" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveJPGInfo.c_str());
        return -1;
    }

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    // 把YUVJ数据帧发给编码器实例
    ret = avcodec_send_frame(jpg_encode_ctx, yuvj_frame);
    while (ret == 0) {
        packet->stream_index = 0;
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(jpg_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE("encode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveJPGInfo = "encode frame occur error  :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveJPGInfo.c_str());
            break;
        }
        ret = av_write_frame(jpg_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveJPGInfo = "write frame occur error  :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveJPGInfo.c_str());
            break;
        }
    }
    av_write_trailer(jpg_fmt_ctx); // 写入文件尾
    av_frame_free(&yuvj_frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(jpg_fmt_ctx->pb); // 关闭输出流
    avcodec_close(jpg_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&jpg_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(jpg_fmt_ctx); // 释放封装器的实例
    return 0;
}


JNIEnv *SaveJPGSwsFromVideo::GetJNIEnv(bool *isAttach) {
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

void SaveJPGSwsFromVideo::PostStatusMessage(const char *msg) {
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