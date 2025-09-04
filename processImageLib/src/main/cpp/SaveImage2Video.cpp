//
// Created by wangyao on 2025/9/4.
//

#include "SaveImage2Video.h"

SaveImage2Video::SaveImage2Video(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveImage2Video::~SaveImage2Video() {
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

void SaveImage2Video::startImage2Video(const char *srcPath1,const char *srcPath2, const char *destPath) {
    sSrcPath1 = srcPath1;
    sSrcPath2 = srcPath2;
    sDestPath = destPath;
    processImageProcedure();
}

void SaveImage2Video::processImageProcedure() {
    if (open_input_file(0, sSrcPath1.c_str()) < 0) { // 打开第一个图片文件
        return;
    }
    if (open_input_file(1, sSrcPath2.c_str()) < 0) { // 打开第二个图片文件
        return;
    }
    if (open_output_file(sDestPath.c_str()) < 0) { // 打开输出文件
        return ;
    }
    if (init_sws_context(0) < 0) { // 初始化第一个图像转换器的实例
        return ;
    }
    if (init_sws_context(1) < 0) { // 初始化第二个图像转换器的实例
        return;
    }

    LOGI("开始转换第一张图片。。。。。。\n");
    image2VideoInfo = "开始转换第一张图片。。。。。。\n";
    PostStatusMessage(image2VideoInfo.c_str());
    int ret = -1;
    // 首先把第一张图片转为视频文件
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx[0], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[0]) { // 视频包需要重新编码
            recode_video(0, packet, frame); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = nullptr; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(0, packet, frame); // 对视频帧重新编码
    LOGI("开始转换第二张图片。。。。。。\n");
    image2VideoInfo = "开始转换第二张图片。。。。。。\n";
    PostStatusMessage(image2VideoInfo.c_str());
    // 然后在视频末尾追加第二张图片
    while (av_read_frame(in_fmt_ctx[1], packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index[1]) { // 视频包需要重新编码
            recode_video(1, packet, frame); // 对视频帧重新编码
        }
        av_packet_unref(packet); // 清除数据包
    }
//    yuv_frame[1]->pts = packet_index++; // 播放时间戳要递增
//    output_video(yuv_frame[1]); // 末尾补上一帧，避免尾巴丢帧
    packet->data = nullptr; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(1, packet, frame); // 对视频帧重新编码
    output_video(nullptr); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success convert image to video.\n");
    image2VideoInfo = "Success convert image to video.\n";
    PostStatusMessage(image2VideoInfo.c_str());
    av_frame_free(&yuv_frame[0]); // 释放数据帧资源
    av_frame_free(&yuv_frame[1]); // 释放数据帧资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(image_decode_ctx[0]); // 关闭视频解码器的实例
    avcodec_free_context(&image_decode_ctx[0]); // 释放视频解码器的实例
    avcodec_close(image_decode_ctx[1]); // 关闭视频解码器的实例
    avcodec_free_context(&image_decode_ctx[1]); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    sws_freeContext(swsContext[0]); // 释放图像转换器的实例
    sws_freeContext(swsContext[1]); // 释放图像转换器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx[0]); // 关闭音视频文件
    avformat_close_input(&in_fmt_ctx[1]); // 关闭音视频文件

}


// 打开输入文件
int SaveImage2Video::open_input_file(int seq, const char *src_name) {
    // 打开图像文件
    int ret = avformat_open_input(&in_fmt_ctx[seq], src_name, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", src_name);
        image2VideoInfo = "Can't open file : " + string(src_name)  + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    LOGI("Success open input_file %s.\n", src_name);
    image2VideoInfo = "Success open input_file : " + string(src_name) + "\n";
    PostStatusMessage(image2VideoInfo.c_str());
    // 查找图像文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx[seq], nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "Can't find stream information. " + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    // 找到视频流的索引
    video_index[seq] = av_find_best_stream(in_fmt_ctx[seq], AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index[seq] >= 0) {
        AVStream *src_video = in_fmt_ctx[seq]->streams[video_index[seq]];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找图像解码器
        AVCodec *video_codec = (AVCodec*) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            image2VideoInfo = "video_codec not found \n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
        image_decode_ctx[seq] = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!image_decode_ctx) {
            LOGE("image_decode_ctx is null\n");
            image2VideoInfo = "image_decode_ctx is null\n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(image_decode_ctx[seq], src_video->codecpar);
        ret = avcodec_open2(image_decode_ctx[seq], video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open image_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            image2VideoInfo = "Can't open image_decode_ctx " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
    } else {
        LOGE("Can't find video stream.\n");
        image2VideoInfo = "Can't find video stream.\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    return 0;
}

// 打开输出文件
int SaveImage2Video::open_output_file(const char *dest_name) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dest_name);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "Can't alloc output_file" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "Can't open output_file " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    LOGI("Success open output_file %s.\n", dest_name);
    image2VideoInfo = "Success open output_file :"+ sDestPath+"\n";
    PostStatusMessage(image2VideoInfo.c_str());
    if (video_index[0] >= 0) { // 创建编码器实例和新的视频流
        src_video = in_fmt_ctx[0]->streams[video_index[0]];
//        // 查找视频编码器
//        AVCodec *video_codec = (AVCodec*) avcodec_find_encoder(AV_CODEC_ID_H264);
        //使用libx264的编码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_encoder_by_name("libx264");
        if (!video_codec) {
            LOGE("video_codec not found\n");
            image2VideoInfo = "video_codec not found \n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            LOGE("video_encode_ctx is null\n");
            image2VideoInfo = "video_encode_ctx is null\n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
        video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P; // 像素格式
        video_encode_ctx->width = src_video->codecpar->width; // 视频宽度
        video_encode_ctx->height = src_video->codecpar->height; // 视频高度
        video_encode_ctx->framerate = (AVRational){25, 1}; // 帧率
        video_encode_ctx->time_base = (AVRational){1, 25}; // 时间基
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
//        video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
        if (ret < 0) {
            LOGE("Can't open video_encode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            image2VideoInfo = "Can't open video_encode_ctx" + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(image2VideoInfo.c_str());
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "write file_header occur error " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    LOGI("Success write file_header.\n");
    image2VideoInfo = "Success write file_header. \n";
    PostStatusMessage(image2VideoInfo.c_str());
    return 0;
}

// 给视频帧编码，并写入压缩后的视频包
int SaveImage2Video::output_video(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0) {
        LOGE("send frame occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "send frame occur error " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return ret;
    }
    while (1) {
        AVPacket *packet = av_packet_alloc(); // 分配一个数据包
        // 从编码器实例获取压缩后的数据包
        ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            LOGE("encode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            image2VideoInfo = "encode frame occur error" + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(image2VideoInfo.c_str());
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            image2VideoInfo = "write frame occur error" + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(image2VideoInfo.c_str());
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 初始化图像转换器的实例
int SaveImage2Video::init_sws_context(int seq) {
    enum AVPixelFormat target_format = AV_PIX_FMT_YUV420P; // 视频的像素格式是YUV
    // 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    swsContext[seq] = sws_getContext(
            image_decode_ctx[seq]->width, image_decode_ctx[seq]->height, image_decode_ctx[seq]->pix_fmt,
            video_encode_ctx->width, video_encode_ctx->height, target_format,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    if (swsContext == nullptr) {
        LOGE("swsContext is null\n");
        image2VideoInfo = "swsContext is null \n";
        PostStatusMessage(image2VideoInfo.c_str());
        return -1;
    }
    yuv_frame[seq] = av_frame_alloc(); // 分配一个YUV数据帧
    yuv_frame[seq]->format = target_format; // 像素格式
    yuv_frame[seq]->width = video_encode_ctx->width; // 视频宽度
    yuv_frame[seq]->height = video_encode_ctx->height; // 视频高度
    // 分配缓冲区空间，用于存放转换后的图像数据
    av_image_alloc(yuv_frame[seq]->data, yuv_frame[seq]->linesize,
                   video_encode_ctx->width, video_encode_ctx->height, target_format, 1);
//    // 分配缓冲区空间，用于存放转换后的图像数据
//    int buffer_size = av_image_get_buffer_size(target_format, video_encode_ctx->width, video_encode_ctx->height, 1);
//    unsigned char *out_buffer = (unsigned char*)av_malloc(
//                            (size_t)buffer_size * sizeof(unsigned char));
//    // 将数据帧与缓冲区关联
//    av_image_fill_arrays(yuv_frame[seq]->data, yuv_frame[seq]->linesize, out_buffer,
//                       target_format, video_encode_ctx->width, video_encode_ctx->height, 1);
    return 0;
}

// 对视频帧重新编码
int SaveImage2Video::recode_video(int seq, AVPacket *packet, AVFrame *frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(image_decode_ctx[seq], packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        image2VideoInfo = "send packet occur error " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(image2VideoInfo.c_str());
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(image_decode_ctx[seq], frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            LOGE("decode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            image2VideoInfo = "decode frame occur error " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(image2VideoInfo.c_str());
            break;
        }
        // 转换器开始处理图像数据，把RGB图像转为YUV图像
        sws_scale(swsContext[seq], (const uint8_t* const*) frame->data, frame->linesize,
                  0, frame->height, yuv_frame[seq]->data, yuv_frame[seq]->linesize);
        int i = 0;
        while (i++ < 100) { // 每张图片占据100个视频帧
            yuv_frame[seq]->pts = packet_index++; // 播放时间戳要递增
            output_video(yuv_frame[seq]); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}


JNIEnv *SaveImage2Video::GetJNIEnv(bool *isAttach) {
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

void SaveImage2Video::PostStatusMessage(const char *msg) {
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