//
// Created by wangyao on 2025/9/13.
//

#include "includes/ProcessVideoToFilm.h"

ProcessVideoToFilm::ProcessVideoToFilm(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

ProcessVideoToFilm::~ProcessVideoToFilm() {
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

void ProcessVideoToFilm::startProcessVideoToFilm(const char *srcPath, const char *outPath,
                                                 const char *filterCmd) {
    sSrcPath = srcPath;
    sOutPath = outPath;
    sFilterCmd = filterCmd;
    ProcessVideoToFilmProcedure();
}

void ProcessVideoToFilm::ProcessVideoToFilmProcedure() {
    if (open_input_file(sSrcPath.c_str()) < 0) { // 打开输入文件
        return;
    }
    if (strlen(sFilterCmd.c_str()) <= 0) { // 构造老电影上下两侧的胶卷边框滤镜
        int width = video_decode_ctx->width; // 视频的宽度
        int add_height = 140; // 添加的高度
        int side = 30; // 方块的边长
        int gap = 20; // 两个方块之间的距离
        int box_count = width / (side + gap); // 小方块的数量
        char film_desc[(box_count + 1) * 60 * 2]; // 老电影的过滤字符串
        // 视频的上下两侧各往外侧延伸出一排黑边
        snprintf(film_desc, sizeof(film_desc), "pad=w=iw:h=ih+%d:x=0:y=%d:color=black", add_height,
                 add_height / 2);
        int i = 0;
        while (i <= box_count) { // 往上下两侧新增的黑边添加白色小方块
            int x_pos = gap + i * (side + gap);
            int x_side = x_pos + side > width ? width - x_pos : side;
            snprintf(film_desc, sizeof(film_desc),
                     "%s,drawbox=x=%d:y=%d:w=%d:h=%d:color=white:t=fill", film_desc, x_pos, gap,
                     side, side);
            snprintf(film_desc, sizeof(film_desc),
                     "%s,drawbox=x=%d:y=ih-%d:w=%d:h=%d:color=white:t=fill", film_desc, x_pos,
                     side + gap, side, side);
            i++;
        }
        init_filter(film_desc); // 初始化滤镜
    } else {
        init_filter(sFilterCmd.c_str()); // 初始化滤镜
    }
    if (open_output_file(sOutPath.c_str()) < 0) { // 打开输出文件
        return;
    }

    int ret = -1;
    AVPacket *packet = av_packet_alloc(); // 分配一个数据包
    AVFrame *frame = av_frame_alloc(); // 分配一个数据帧
    AVFrame *filt_frame = av_frame_alloc(); // 分配一个过滤后的数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            packet->stream_index = 0;
            recode_video(packet, frame, filt_frame); // 对视频帧重新编码
        } else if (packet->stream_index == audio_index) { // 音频包暂不重新编码，直接写入目标文件
            packet->stream_index = 1;
            ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
            if (ret < 0) {
                LOGE("write frame occur error %d.\n", ret);
                av_strerror(ret, errbuf, sizeof(errbuf));
                videoFilterInfo =
                        "write frame occur error:" + to_string(ret) + "\n error msg：" +
                        string(errbuf) + "\n";
                PostStatusMessage(videoFilterInfo.c_str());
                break;
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    packet->data = NULL; // 传入一个空包，冲走解码缓存
    packet->size = 0;
    recode_video(packet, frame, filt_frame); // 对视频帧重新编码
    output_video(NULL); // 传入一个空帧，冲走编码缓存
    av_write_trailer(out_fmt_ctx); // 写文件尾
    LOGI("Success process film file.\n");
    videoFilterInfo =
            "Success process film file.:" + sOutPath + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
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
int ProcessVideoToFilm::open_input_file(const char *src_name) {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", src_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Can't open file :" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success open input_file %s.\n", src_name);
    videoFilterInfo =
            "Success open input_file :" + sSrcPath + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, NULL);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Can't open file :" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    // 找到视频流的索引
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, NULL, 0);
    if (video_index >= 0) {
        src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            videoFilterInfo =
                    "video_codec not found \n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            videoFilterInfo =
                    "video_decode_ctx is null \n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, NULL); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo =
                    "Can't open video_decode_ctx :" + to_string(ret) + "\n error msg：" +
                    string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
    } else {
        LOGE("Can't find video stream.\n");
        videoFilterInfo =
                "Can't find video stream.\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    // 找到音频流的索引
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, NULL, 0);
    if (audio_index >= 0) {
        src_audio = in_fmt_ctx->streams[audio_index];
    }
    return 0;
}


// 打开输出文件
int ProcessVideoToFilm::open_output_file(const char *dest_name) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, NULL, NULL, dest_name);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Can't alloc output_file :" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", dest_name);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Can't open output_file :" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success open output_file %s.\n", dest_name);
    videoFilterInfo =
            "Success open output_file :" + string(dest_name) + "\n";
    PostStatusMessage(videoFilterInfo.c_str());

    if (video_index >= 0) { // 创建编码器实例和新的视频流
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频编码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_encoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            videoFilterInfo =
                    "video_codec not found.\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编码器的实例
        if (!video_encode_ctx) {
            LOGE("video_encode_ctx is null \n");
            videoFilterInfo =
                    "video_encode_ctx is null .\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx); // 帧率
        video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx); // 时间基
        video_encode_ctx->gop_size = 12; // 关键帧的间隔距离
        video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx); // 视频宽度
        video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx); // 视频高度
        // 视频的像素格式（颜色空间）
        video_encode_ctx->pix_fmt = (enum AVPixelFormat) av_buffersink_get_format(buffersink_ctx);
        //video_encode_ctx->max_b_frames = 0; // 0表示不要B帧
        // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
        if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
            video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
        }
        ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
        if (ret < 0) {
            LOGE("Can't open video_encode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo =
                    "Can't open video_encode_ctx:" + to_string(ret) + "\n error msg：" +
                    string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            return -1;
        }
        dest_video = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把编码器实例的参数复制给目标视频流
        avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
        dest_video->codecpar->codec_tag = 0;
    }
    if (audio_index >= 0) { // 源文件有音频流，就给目标文件创建音频流
        AVStream *dest_audio = avformat_new_stream(out_fmt_ctx, nullptr); // 创建数据流
        // 把源文件的音频参数原样复制过来
        avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
        dest_audio->codecpar->codec_tag = 0;
    }
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "write file_header occur errorx:" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return -1;
    }
    LOGI("Success write file_header.\n");
    videoFilterInfo =
            "Success write file_header. \n";
    PostStatusMessage(videoFilterInfo.c_str());
    return 0;
}

// 初始化滤镜（也称过滤器、滤波器）
int ProcessVideoToFilm::init_filter(const char *filters_desc) {
    LOGI("filters_desc : %s\n", filters_desc);
    videoFilterInfo =
            "filters_desc:" + string(filters_desc) + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
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
    videoFilterInfo =
            "args :" + string(args) + "\n";
    PostStatusMessage(videoFilterInfo.c_str());
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, NULL, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer source\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Cannot create buffer source" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       NULL, NULL, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer sink\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Cannot create buffer sink" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        LOGE("Cannot set output pixel format\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Cannot set output pixel format" + to_string(ret) + "\n error msg：" +
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
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, NULL);
    if (ret < 0) {
        LOGE("Cannot parse graph string\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Cannot parse graph string" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, NULL);
    if (ret < 0) {
        LOGE("Cannot config filter graph\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "Cannot config filter graph" + to_string(ret) + "\n error msg：" +
                string(errbuf) + "\n";
        PostStatusMessage(videoFilterInfo.c_str());
        return ret;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    LOGI("Success initialize filter.\n");
    videoFilterInfo =
            "Success initialize filter.\n";
    PostStatusMessage(videoFilterInfo.c_str());
    return ret;
}

// 给视频帧编码，并写入压缩后的视频包
int ProcessVideoToFilm::output_video(AVFrame *frame) {
    // 把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, frame);
    if (ret < 0) {
        LOGE("send frame occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "send frame occur error :" + to_string(ret) + "\n error msg：" +
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
            videoFilterInfo =
                    "encode frame occur error:" + to_string(ret) + "\n error msg：" +
                    string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        if (packet != nullptr && packet->buf != nullptr) {
            videoFilterInfo =
                    "往文件写入大小：" + to_string(packet->buf->size) + "的数据包...\n";
            PostStatusMessage(videoFilterInfo.c_str());
        }
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo =
                    "write frame occur error :" + to_string(ret) + "\n error msg：" +
                    string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

// 对视频帧重新编码
int ProcessVideoToFilm::recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame) {
    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        videoFilterInfo =
                "send packet occur error  :" + to_string(ret) + "\n error msg：" +
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
            videoFilterInfo =
                    "decode frame occur error :" + to_string(ret) + "\n error msg：" +
                    string(errbuf) + "\n";
            PostStatusMessage(videoFilterInfo.c_str());
            break;
        }
        // 把原始的数据帧添加到输入滤镜的缓冲区
        ret = av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        if (ret < 0) {
            LOGE("Error while feeding the filtergraph\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            videoFilterInfo =
                    "Error while feeding the filtergraph:" + to_string(ret) + "\n error msg：" +
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
                videoFilterInfo =
                        "get buffersink frame occur error:" + to_string(ret) + "\n error msg：" +
                        string(errbuf) + "\n";
                PostStatusMessage(videoFilterInfo.c_str());
                break;
            }
            output_video(filt_frame); // 给视频帧编码，并写入压缩后的视频包
        }
    }
    return ret;
}


JNIEnv *ProcessVideoToFilm::GetJNIEnv(bool *isAttach) {
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

void ProcessVideoToFilm::PostStatusMessage(const char *msg) {
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