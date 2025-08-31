//
// Created by WANGYAO on 2025/8/31.
//

#include "includes/WriteYUVFrame.h"

WriteYUVFrame::WriteYUVFrame(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

WriteYUVFrame::~WriteYUVFrame() {

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

    if (mThread != nullptr) {
        mThread->join();
        delete mThread;
        mThread = nullptr;
    }

    if (out_fmt_ctx) {
        out_fmt_ctx = nullptr;
    }

    if (dest_video) {
        dest_video = nullptr;
    }

    if (video_encode_ctx) {
        video_encode_ctx = nullptr;
    }

    if (mFrame) {
        mFrame = nullptr;
    }

    mDestPath = nullptr;

}


void WriteYUVFrame::startWriteYUVThread(const char *destPath) {
    LOGE("startWriteYUVThread %s.\n", destPath);
    mDestPath = destPath;
    if (open_output_file(destPath) < 0) { // 打开输出文件
        return;
    }
    if (mThread == nullptr) {
        mThread = new thread(DoThreading, this);
        mThread->detach();
    }
}

void WriteYUVFrame::DoThreading(WriteYUVFrame *writeYUV) {
    writeYUV->processImageProcedure();
}

void WriteYUVFrame::processImageProcedure() {
    LOGE("processImageProcedure .\n");

    mFrame = av_frame_alloc(); // 分配一个数据帧
    mFrame->format = video_encode_ctx->pix_fmt; // 像素格式
    mFrame->width = video_encode_ctx->width; // 视频宽度
    mFrame->height = video_encode_ctx->height; // 视频高度
    int ret = av_frame_get_buffer(mFrame, 0); // 为数据帧分配缓冲区
    if (ret < 0) {
        LOGE("Can't allocate frame data %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "Can't allocate frame data:" + to_string(ret) + "\n error msg：" +
                       string(errbuf) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
        return;
    }
    int index = 0;
    while (index < 200) { // 写入200帧
        ret = av_frame_make_writable(mFrame); // 确保数据帧是可写的
        if (ret < 0) {
            LOGE("Can't make frame writable %d.\n", ret);
            av_strerror(ret, errbuf, sizeof(errbuf));
            writeYUVInfo = "Can't make frame writable:" + to_string(ret) + "\n error msg：" +
                           string(errbuf) + "\n";
            PostStatusMessage(writeYUVInfo.c_str());
            return;
        }

        int x, y;
        // 写入Y值
        for (y = 0; y < video_encode_ctx->height; y++)
            for (x = 0; x < video_encode_ctx->width; x++)
                mFrame->data[0][y * mFrame->linesize[0] + x] = 0; // Y值填0
        // 写入U值（Cb）和V值（Cr）
        for (y = 0; y < video_encode_ctx->height / 2; y++) {
            for (x = 0; x < video_encode_ctx->width / 2; x++) {
                mFrame->data[1][y * mFrame->linesize[1] + x] = 0; // U值填0
                mFrame->data[2][y * mFrame->linesize[2] + x] = 0; // V值填0
            }
        }
        mFrame->pts = index++; // 时间戳递增
        output_video(mFrame); // 给视频帧编码，并写入压缩后的视频包
    }
    LOGI("Success write yuv video.\n");
    writeYUVInfo = "Success write yuv video.\n";
    PostStatusMessage(writeYUVInfo.c_str());
    av_write_trailer(out_fmt_ctx); // 写文件尾

    av_frame_free(&mFrame); // 释放数据帧资源
    avio_close(out_fmt_ctx->pb); // 关闭输出流
    avcodec_close(video_encode_ctx); // 关闭视频编码器的实例
    avcodec_free_context(&video_encode_ctx); // 释放视频编码器的实例
    avformat_free_context(out_fmt_ctx); // 释放封装器的实例
}


// 给视频帧编码，并写入压缩后的视频包
int WriteYUVFrame::output_video(AVFrame *frame) {
    //把原始的数据帧发给编码器实例
    int ret = avcodec_send_frame(video_encode_ctx, mFrame);
    if (ret < 0) {
        LOGE("send frame occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "send frame occur error:" + to_string(ret) + "\n error msg：" +
                       string(errbuf) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
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
            writeYUVInfo = "encode frame occur error:" + to_string(ret) + "\n error msg：" +
                           string(errbuf) + "\n";
            PostStatusMessage(writeYUVInfo.c_str());
            break;
        }
        if (packet->buf) {
            writeYUVInfo =
                    "写入packet的大小：" + to_string(packet->buf->size) + "\n";
            PostStatusMessage(writeYUVInfo.c_str());
        }
        // 把数据包的时间戳从一个时间基转换为另一个时间基
        av_packet_rescale_ts(packet, video_encode_ctx->time_base, dest_video->time_base);
        packet->stream_index = 0;
        ret = av_write_frame(out_fmt_ctx, packet); // 往文件写入一个数据包
        if (ret < 0) {
            LOGE("write frame occur error %d.\n", ret);
            writeYUVInfo = "write frame occur error:" + to_string(ret) + "\n error msg：" +
                           string(errbuf) + "\n";
            PostStatusMessage(writeYUVInfo.c_str());
            break;
        }
        av_packet_unref(packet); // 清除数据包
    }
    return ret;
}

int WriteYUVFrame::open_output_file(const char *destPath) {
    // 分配音视频文件的封装实例
    int ret = avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, mDestPath);
    if (ret < 0) {
        LOGE("Can't alloc output_file %s.\n", mDestPath);
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "Can't alloc output_file:" + string(mDestPath) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    // 打开输出流
    ret = avio_open(&out_fmt_ctx->pb, mDestPath, AVIO_FLAG_READ_WRITE);
    if (ret < 0) {
        LOGE("Can't open output_file %s.\n", mDestPath);
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "Can't open output_file:" + to_string(ret) + "\n error msg：" +
                       string(errbuf) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    LOGI("Success open output_file %s.\n", mDestPath);
    writeYUVInfo = "Success open output_file:" + string(mDestPath);
    PostStatusMessage(writeYUVInfo.c_str());
    // 查找编码器
    AVCodec *video_codec = (AVCodec *) avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!video_codec) {
        LOGE("AV_CODEC_ID_H264 not found\n");
        writeYUVInfo = "AV_CODEC_ID_H264 not found \n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    video_encode_ctx = avcodec_alloc_context3(video_codec); // 分配编解码器的实例
    if (!video_encode_ctx) {
        LOGE("video_encode_ctx is null\n");
        writeYUVInfo = "video_encode_ctx is null \n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    video_encode_ctx->pix_fmt = AV_PIX_FMT_YUV420P; // 像素格式
    video_encode_ctx->width = 480; // 视频画面的宽度
    video_encode_ctx->height = 270; // 视频画面的高度
//    video_encode_ctx->width = 1440; // 视频画面的宽度
//    video_encode_ctx->height = 810; // 视频画面的高度。高度大等于578，播放器默认色度BT709；高度小等于576，播放器默认色度BT601
    video_encode_ctx->framerate = (AVRational) {25, 1}; // 帧率
    video_encode_ctx->time_base = (AVRational) {1, 25}; // 时间基
    // AV_CODEC_FLAG_GLOBAL_HEADER标志允许操作系统显示该视频的缩略图
    if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    ret = avcodec_open2(video_encode_ctx, video_codec, nullptr); // 打开编码器的实例
    if (ret < 0) {
        LOGE("Can't open video_encode_ctx.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "Can't open video_encode_ctx:" + to_string(ret) + "\n error msg：" +
                       string(errbuf) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    // 创建指定编码器的数据流
    dest_video = avformat_new_stream(out_fmt_ctx, video_codec);
    // 把编码器实例中的参数复制给数据流
    avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
    dest_video->codecpar->codec_tag = 0; // 非特殊情况都填0
    ret = avformat_write_header(out_fmt_ctx, nullptr); // 写文件头
    if (ret < 0) {
        LOGE("write file_header occur error %d.\n", ret);
        av_strerror(ret, errbuf, sizeof(errbuf));
        writeYUVInfo = "write file_header occur error:" + to_string(ret) + "\n error msg：" +
                       string(errbuf) + "\n";
        PostStatusMessage(writeYUVInfo.c_str());
        return -1;
    }
    LOGI("Success write file_header.\n");
    writeYUVInfo = "Success write file_header\n";
    PostStatusMessage(writeYUVInfo.c_str());
    return 0;
}


JNIEnv *WriteYUVFrame::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGD("RecodecVideo::GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGD("RecodecVideo::GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void WriteYUVFrame::PostStatusMessage(const char *msg) {
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





