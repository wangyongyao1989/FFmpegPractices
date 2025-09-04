//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/4.
//

#include "SaveGifSwsOfVideo.h"

SaveGifSwsOfVideo::SaveGifSwsOfVideo(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveGifSwsOfVideo::~SaveGifSwsOfVideo() {

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


}

void SaveGifSwsOfVideo::startWriteGif(const char *srcPath, const char *destPath) {
    sSrcPath = srcPath;
    sDestPath = destPath;
    processImageProcedure();
}

void SaveGifSwsOfVideo::processImageProcedure() {
    if (open_input_file(sSrcPath.c_str()) < 0) { // 打开输入文件
        return;
    }
    if (open_output_file(sDestPath.c_str()) < 0) { // 打开输出文件
        return;
    }
    LOGI("target image file is %s\n", sDestPath.c_str());
    saveGifInfo = "target image file is " + sDestPath + "\n";
    PostStatusMessage(saveGifInfo.c_str());

    if (init_sws_context() < 0) { // 初始化图像转换器的实例
        return;
    }

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            packet_index++;
            decode_video(packet, frame, packet_index); // 对视频帧解码
            if (packet_index > save_pro_index) { // 已经采集到足够数量的帧
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    LOGI("Success save %d_index frame as gif file.\n", save_pro_index);
    saveGifInfo = "Success save %d_index frame as gif file \n";
    PostStatusMessage(saveGifInfo.c_str());

    av_write_trailer(gif_fmt_ctx); // 写入文件尾
    sws_freeContext(swsContext); // 释放图像转换器的实例
    avio_close(gif_fmt_ctx->pb); // 关闭输出流
    av_frame_free(&rgb_frame); // 释放数据帧资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(gif_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&gif_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(gif_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件

}

// 打开输入文件
int SaveGifSwsOfVideo::open_input_file(const char *srcPath) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, srcPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", srcPath);
        saveGifInfo = "Can't open file ：" + string(srcPath) + ".\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    LOGI("Success open input_file %s.\n", srcPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "Can't find stream information. " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    // 找到视频流的索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            saveGifInfo = "video_codec not found.\n";
            PostStatusMessage(saveGifInfo.c_str());
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            saveGifInfo = "video_decode_ctx is null.\n";
            PostStatusMessage(saveGifInfo.c_str());
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveGifInfo = "Can't open video_decode_ctx" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveGifInfo.c_str());
            return -1;
        }
    } else {
        LOGE("Can't find video stream.\n");
        saveGifInfo = "Can't find video stream.\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    return 0;
}

// 打开输出文件
int SaveGifSwsOfVideo::open_output_file(const char *gif_name) {
    // 分配GIF文件的封装实例
    int ret = avformat_alloc_output_context2(&gif_fmt_ctx, nullptr, nullptr, gif_name);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", gif_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "Can't alloc output_file :" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    // 打开输出流
    ret = avio_open(&gif_fmt_ctx->pb, gif_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", gif_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "Can't open output_file :" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    // 查找GIF编码器
    AVCodec *gif_codec = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_GIF);
    if (!gif_codec) {
        LOGE("gif_codec not found\n");
        saveGifInfo = "gif_codec not found\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    // 获取编解码器上下文信息
    gif_encode_ctx = avcodec_alloc_context3(gif_codec);
    if (!gif_encode_ctx) {
        LOGE("gif_encode_ctx is null\n");
        saveGifInfo = "gif_encode_ctx is null\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    gif_encode_ctx->pix_fmt = target_format; // 像素格式
    gif_encode_ctx->width = video_decode_ctx->width; // 视频宽度
    gif_encode_ctx->height = video_decode_ctx->height; // 视频高度
    gif_encode_ctx->time_base = src_video->time_base; // 时间基
    LOGI("gif codec_id = %d\n", gif_encode_ctx->codec_id);
    ret = avcodec_open2(gif_encode_ctx, gif_codec, nullptr); // 打开编码器的实例
    if (ret < 0) {
        LOGE("Can't open gif_encode_ctx.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "Can't open gif_encode_ctx:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    gif_stream = avformat_new_stream(gif_fmt_ctx, 0); // 创建数据流
    // 把编码器实例的参数复制给目标视频流
    avcodec_parameters_from_context(gif_stream->codecpar, gif_encode_ctx);
    gif_stream->codecpar->codec_tag = 0;
    ret = avformat_write_header(gif_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "write file_header occur error:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    return 0;
}

int SaveGifSwsOfVideo::init_sws_context(void) {
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    swsContext = sws_getContext(
            video_decode_ctx->width, video_decode_ctx->height, AV_PIX_FMT_YUV420P,
            video_decode_ctx->width, video_decode_ctx->height, target_format,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext == nullptr) {
        LOGE("swsContext is null\n");
        saveGifInfo = "swsContext is null\n";
        PostStatusMessage(saveGifInfo.c_str());
        return -1;
    }
    rgb_frame = av_frame_alloc(); // 分配一个RGB数据帧
    rgb_frame->format = target_format; // 像素格式
    rgb_frame->width = video_decode_ctx->width; // 视频宽度
    rgb_frame->height = video_decode_ctx->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(rgb_frame->data, rgb_frame->linesize,
                   video_decode_ctx->width, video_decode_ctx->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, video_decode_ctx->width, video_decode_ctx->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(rgb_frame->data, rgb_frame->linesize, out_buffer, target_format,
//                       video_decode_ctx->width, video_decode_ctx->height, 1);
    return 0;
}

// 对视频帧解码。save_index表示要把第几个视频帧保存为图片
int SaveGifSwsOfVideo::decode_video(AVPacket *packet, AVFrame *frame, int save_index) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveGifInfo = "send packet occur error:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveGifInfo.c_str());
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
            saveGifInfo = "decode frame occur error:" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveGifInfo.c_str());
            break;
        }
        // 转换器开始处理图像数据，把YUV图像转为RGB图像
        sws_scale(swsContext, (const uint8_t *const *) frame->data, frame->linesize,
                  0, frame->height, rgb_frame->data, rgb_frame->linesize);
        rgb_frame->pts = frame->pts; // 设置数据帧的播放时间戳
        save_gif_file(rgb_frame, save_index); // 把视频帧保存为GIF图片
        break;
    }
    return ret;
}

// 把视频帧保存为GIF图片。save_index表示要把第几个视频帧保存为图片
// 这个警告不影响gif生成：No accelerated colorspace conversion found from yuv420p to bgr8
int SaveGifSwsOfVideo::save_gif_file(AVFrame *frame, int save_index) {
    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    LOGI("format = %d, width = %d, height = %d\n",
         frame->format, frame->width, frame->height);
    saveGifInfo = "format =" + to_string(frame->format) + ",width =" + to_string(frame->width) +
                  ",height =" + to_string(frame->width) + "\n";
    saveGifInfo = saveGifInfo + "现在保存的是第:" + to_string(save_index) + "帧数据。。。。。的\n";
    PostStatusMessage(saveGifInfo.c_str());

    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(gif_encode_ctx, frame);
    while (ret == 0) {
        packet->stream_index = 0;
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(gif_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE("encode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveGifInfo = "encode frame occur error :" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveGifInfo.c_str());
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, src_video->time_base, gif_stream->time_base);
        ret = av_write_frame(gif_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveGifInfo = "write frame occur error:" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveGifInfo.c_str());
            break;
        }
    }
    av_packet_unref(packet); // 清除数据包
    return 0;
}


JNIEnv *SaveGifSwsOfVideo::GetJNIEnv(bool *isAttach) {
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

void SaveGifSwsOfVideo::PostStatusMessage(const char *msg) {
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
