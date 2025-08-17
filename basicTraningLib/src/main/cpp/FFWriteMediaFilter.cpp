//
// Created by wangyao on 2025/8/16.
//

#include "FFWriteMediaFilter.h"

string FFWriteMediaFilter::init_filter(AVStream *video_stream, AVCodecContext *video_decode_ctx,
                                       const char *filters_desc) {
    int ret = 0;
    string retFilterMsg;
    const AVFilter *buffersrc = avfilter_get_by_name("buffer"); // 获取输入滤镜
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 获取输出滤镜
    AVFilterInOut *inputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVFilterInOut *outputs = avfilter_inout_alloc(); // 分配滤镜的输入输出参数
    AVRational time_base = video_stream->time_base;
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    filter_graph = avfilter_graph_alloc(); // 分配一个滤镜图
    if (!outputs || !inputs || !filter_graph) {
        retFilterMsg = &"AVERROR(ENOMEM):" [ AVERROR(ENOMEM)];
        return retFilterMsg;
    }
    char args[512]; // 临时字符串，存放输入源的媒体参数信息，比如视频的宽高、像素格式等
    snprintf(args, sizeof(args),
             "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
             video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
             time_base.num, time_base.den,
             video_decode_ctx->sample_aspect_ratio.num, video_decode_ctx->sample_aspect_ratio.den);
    LOGI("args : %s\n", args);
    // 创建输入滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in",
                                       args, nullptr, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer source\n");
        retFilterMsg = "Cannot create buffer source";
        return retFilterMsg;
    }
    // 创建输出滤镜的实例，并将其添加到现有的滤镜图
    ret = avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out",
                                       nullptr, nullptr, filter_graph);
    if (ret < 0) {
        LOGE("Cannot create buffer sink\n");
        retFilterMsg = "Cannot create buffer sink";
        return retFilterMsg;
    }
    // 将二进制选项设置为整数列表，此处给输出滤镜的实例设置像素格式
    ret = av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts,
                              AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    if (ret < 0) {
        LOGE("Cannot set output pixel format\n");
        retFilterMsg = "Cannot set output pixel format";
        return retFilterMsg;
    }
    // 设置滤镜的输入输出参数
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    outputs->pad_idx = 0;
    outputs->next = nullptr;
    // 设置滤镜的输入输出参数
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    inputs->pad_idx = 0;
    inputs->next = nullptr;
    // 把采用过滤字符串描述的图形添加到滤镜图
    ret = avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, nullptr);
    if (ret < 0) {
        LOGE("Cannot parse graph string\n");
        retFilterMsg = "Cannot parse graph string";
        return retFilterMsg;
    }
    // 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式
    ret = avfilter_graph_config(filter_graph, nullptr);
    if (ret < 0) {
        LOGE("Cannot config filter graph\n");
        retFilterMsg = "Cannot config filter graph";
        return retFilterMsg;
    }
    avfilter_inout_free(&inputs); // 释放滤镜的输入参数
    avfilter_inout_free(&outputs); // 释放滤镜的输出参数
    LOGI("Success initialize filter.\n");
    retFilterMsg = "Success initialize filter.";
    return retFilterMsg;

}

string FFWriteMediaFilter::writeMediaFilter(const char *filterVideoPath) {

    string retMsg;
    // 打开音视频文件
    int ret = avformat_open_input(&fmt_ctx, filterVideoPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.", filterVideoPath);
        retMsg = "Can't open file ";
        return retMsg;
    }
    LOGI("Success open input_file %s.\n", filterVideoPath);
    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        retMsg = "Can't find stream information";
        return retMsg;
    }
    LOGI("Success find stream information.\n");

    // 找到视频流的索引
    int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    LOGI("video_index=%d\n", video_index);
    if (video_index >= 0) {
        AVStream *video_stream = fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = video_stream->codecpar->codec_id;
        LOGI("video_stream codec_id=%d\n", video_codec_id);
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            retMsg = "video_codec not found";
            return retMsg;
        }
        LOGI("video_codec name=%s\n", video_codec->name);
        LOGI("video_codec long_name=%s\n", video_codec->long_name);
        // 下面的type字段来自AVMediaType定义，为0表示AVMEDIA_TYPE_VIDEO，为1表示AVMEDIA_TYPE_AUDIO
        LOGI("video_codec type=%d\n", video_codec->type);

        AVCodecContext *video_decode_ctx = nullptr; // 视频解码器的实例
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGI("video_decode_ctx is null\n");
            retMsg = "video_decode_ctx is null";
            return retMsg;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
        LOGI("Success copy video parameters_to_context.\n");
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        LOGI("Success open video codec.\n");
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            retMsg = "Can't open video_decode_ctx.";
            return retMsg;
        }
        // 初始化滤镜
        const string &initFilter = init_filter(video_stream, video_decode_ctx, "fps=25");
        retMsg = initFilter;
        avcodec_close(video_decode_ctx); // 关闭解码器的实例
        avcodec_free_context(&video_decode_ctx); // 释放解码器的实例
        avfilter_free(buffersrc_ctx); // 释放输入滤镜的实例
        avfilter_free(buffersink_ctx); // 释放输出滤镜的实例
        avfilter_graph_free(&filter_graph); // 释放滤镜图资源
    } else {
        LOGE("Can't find video stream.\n");
        retMsg = "Can't find video stream.";
        return retMsg;
    }

    avformat_close_input(&fmt_ctx); // 关闭音视频文件
    return retMsg;
}

FFWriteMediaFilter::FFWriteMediaFilter() {

}

FFWriteMediaFilter::~FFWriteMediaFilter() {

}


