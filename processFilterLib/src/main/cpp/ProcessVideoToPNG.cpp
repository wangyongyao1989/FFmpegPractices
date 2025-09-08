//
// Created by WANGYAO on 2025/9/8.
//

#include "includes/ProcessVideoToPNG.h"

ProcessVideoToPNG::ProcessVideoToPNG(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

ProcessVideoToPNG::~ProcessVideoToPNG() {
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

void ProcessVideoToPNG::startProcessVideoToPNG(const char *srcPath, const char *outPath,
                                               const char *filterCmd) {
    sSrcPath = srcPath;
    sOutPath = outPath;
    sFilterCmd = filterCmd;
    processVideoFilterProcedure();
}

void ProcessVideoToPNG::processVideoFilterProcedure() {
    const char *colorspace = "bt709";
    if (open_input_file(sSrcPath.c_str(), colorspace) < 0) { // 打开输入文件
        return;
    }

    init_filter(filters_desc); // 初始化滤镜
    if (open_output_file(sOutPath.c_str()) < 0) { // 打开输出文件
        return;
    }

    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            recode_video(packet, frame, filt_frame); // 对视频帧重新编码
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success process png file.\n");

    avfilter_free(buffersrc_ctx); // 释放输入滤镜的实例
    avfilter_free(buffersink_ctx); // 释放输出滤镜的实例
    avfilter_graph_free(&filter_graph); // 释放滤镜图资源
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件

}

// 打开输入文件
int ProcessVideoToPNG::open_input_file(const char *src_name, const char *colorspace) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", src_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Can't open file .." + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success open input_file %s.\n", src_name);
    videoFilterInfo = "Success open input_file ：" + string(src_name) + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Can't find stream information..." + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
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
            videoFilterInfo = "video_codec not found\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            videoFilterInfo = "video_decode_ctx is null \n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        AVDictionary *codec_options = NULL;
        av_dict_set(&codec_options, "colorspace", colorspace, 0);
        ret = avcodec_open2(video_decode_ctx, video_codec, &codec_options);
        // 打开解码器的实例
        //ret = avcodec_open2(video_decode_ctx, video_codec, NULL);
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo = "Can't open video_decode_ctx...." + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
    } else {
        LOGE("Can't find video stream.\n");
        videoFilterInfo = "Can't find video stream.\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    return 0;
}

// 初始化滤镜（也称过滤器、滤波器）
int ProcessVideoToPNG::init_filter(const char *filters_desc) {
    LOGI("filters_desc : %s\n", filters_desc);
    int ret = 0;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer"); // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    filter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!outputs || !inputs || !filter_graph) {
        ret = AVERROR(ENOMEM);
        return ret;
    }
    char args[512]; // 临时字符串，存放输入源的媒体参数信息，比如视频的宽高、像素格式等
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
             src_video->time_base.num, src_video->time_base.den,
             video_decode_ctx->sample_aspect_ratio.num, video_decode_ctx->sample_aspect_ratio.den);
    LOGI("args : %s\n", args);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, nullptr, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer source\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Cannot create buffer source.." + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer sink\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Cannot create buffer sink." + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = NULL;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = NULL;
    // 把采用过滤字符串描述的图形添加到滤镜图
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0) {
        LOGE("Cannot parse graph string\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Cannot parse graph string" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0) {
        LOGE("Cannot config filter graph \n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Cannot config filter graph " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    LOGI("Success initialize filter.\n");
    videoFilterInfo = "Success initialize filter.\n";
    PostStatusMessage(videoFilterInfo.c_str());
    return ret;
}

// 打开输出文件
int ProcessVideoToPNG::open_output_file(const char *dest_name) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dest_name);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Can't alloc output_file " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "Can't open output_file  " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success open output_file %s.\n", dest_name);
    videoFilterInfo = "Success initialize filter." + string(dest_name) + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
    if (video_index >= 0) { // 创建编码器实例和新的视频流
        // 查找图片编码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_PNG);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            videoFilterInfo = "video_codec not found \n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            LOGE("video_encode_ctx is null \n");
            videoFilterInfo = "video_encode_ctx is null \n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_encode_ctx->pix_fmt = AV_PIX_FMT_RGB24; // png的像素格式
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx); // 画面宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx); // 画面高度
        video_encode_ctx->time_base = (AVRational) {1, 25}; // 时间基
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
        if (ret < 0) {
            LOGE("Can't open video_encode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo = "Can't open video_encode_ctx. " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
        const char *pix_names = av_get_pix_fmt_name(video_encode_ctx->pix_fmt);
        LOGI("pix_names: %s\n", pix_names);
        videoFilterInfo = "pix_names: ." + string(pix_names) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
    }
    ret = avformat_write_header(out_fmt_ctx, NULL); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "write file_header occur error. " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success write file_header.\n");
    videoFilterInfo = "Success write file_header: ." + sOutPath + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
    return 0;
}


// 给视频帧编码，并写入压缩后的视频包
int ProcessVideoToPNG::output_video(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0) {
        LOGE("send frame occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "send frame occur error. " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
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
            videoFilterInfo = "encode frame occur error  " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo = "write frame occur error  " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对视频帧重新编码
int ProcessVideoToPNG::recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo = "send packet occur error " + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        } else if (ret < 0) {
            LOGE("decode frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo = "decode frame occur error " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        // 把原始的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            LOGE("Error while feeding the filtergraph\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo = "Error while feeding the filtergraph " + to_string(ret) + "\n error msg：" +
                              string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        while (1) {
            // 从输出滤镜的接收器获取一个已加工的过滤帧
            ret = av_buffersink_get_frame(buffersink_ctx, filt_frame);
            if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
                break;
            } else if (ret < 0) {
                LOGE("get buffersink frame occur error %d.\n", ret);
                av_strerror(ret, errbuf, sizeof(errbuf));
                videoFilterInfo = "get buffersink frame occur error" + to_string(ret) + "\n error msg：" +
                                  string(errbuf) + "\n";
                PostStatusMessage(videoFilterInfo.c_str());
                break;
            }
            output_video(filt_frame); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}


JNIEnv *ProcessVideoToPNG::GetJNIEnv(bool *isAttach) {
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

void ProcessVideoToPNG::PostStatusMessage(const char *msg) {
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