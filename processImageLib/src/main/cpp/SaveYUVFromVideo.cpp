//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/1.
//

#include "includes/SaveYUVFromVideo.h"

SaveYUVFromVideo::SaveYUVFromVideo(JNIEnv *env, jobject thiz) {
    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);
}

SaveYUVFromVideo::~SaveYUVFromVideo() {
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

    if (in_fmt_ctx) {
        in_fmt_ctx = nullptr;
    }

    if (video_decode_ctx) {
        video_decode_ctx = nullptr;
    }

    if (packet) {
        packet = nullptr;
    }

    packet_index = -1;

}

void SaveYUVFromVideo::startWriteYUVThread(const char *srcPath, const char *destPath) {
    LOGE("startWriteYUVThread destPath %s.\n", destPath);
    mSrcPath = srcPath;
    mDestPath = destPath;
    if (mThread == nullptr) {
        mThread = new thread(DoThreading, this);
        mThread->detach();
    }
}


void SaveYUVFromVideo::DoThreading(SaveYUVFromVideo *saveYUV) {
    saveYUV->processImageProcedure();
}

void SaveYUVFromVideo::processImageProcedure() {
    // 打开音视频文件
    int ret = avformat_open_input(&in_fmt_ctx, mSrcPath, nullptr, nullptr);
    if (ret < 0) {
        LOGE("Can't open file %s.\n", mSrcPath);
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveYUVInfo = "Can't open file:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveYUVInfo.c_str());
        return;
    }
    LOGE("processImageProcedure11 destPath %s.\n", mDestPath);

    LOGI("Success open input_file %s.\n", mSrcPath);
    saveYUVInfo = "Success open input_file:" + string(mSrcPath) + "\n";
    PostStatusMessage(saveYUVInfo.c_str());

    // 查找音视频文件中的流信息
    ret = avformat_find_stream_info(in_fmt_ctx, nullptr);
    if (ret < 0) {
        LOGE("Can't find stream information.\n");
        av_strerror(ret, errbuf, sizeof(errbuf));
        saveYUVInfo = "Can't find stream information:" + to_string(ret) + "\n error msg：" +
                      string(errbuf) + "\n";
        PostStatusMessage(saveYUVInfo.c_str());
        return;
    }
    LOGE("processImageProcedure22 destPath %s.\n", mSrcPath);
    LOGE("processImageProcedure22 srcPath %s.\n", mSrcPath);

    // 找到视频流的索引
    int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (video_index >= 0) {
        AVStream *src_video = in_fmt_ctx->streams[video_index];
        enum AVCodecID video_codec_id = src_video->codecpar->codec_id;
        // 查找视频解码器
        AVCodec *video_codec = (AVCodec *) avcodec_find_decoder(video_codec_id);
        if (!video_codec) {
            LOGE("video_codec not found\n");
            saveYUVInfo = "video_codec not found\n";
            PostStatusMessage(saveYUVInfo.c_str());
            return;
        }
        video_decode_ctx = avcodec_alloc_context3(video_codec); // 分配解码器的实例
        if (!video_decode_ctx) {
            LOGE("video_decode_ctx is null\n");
            saveYUVInfo = "video_decode_ctx is null \n";
            PostStatusMessage(saveYUVInfo.c_str());
            return;
        }
        // 把视频流中的编解码参数复制给解码器的实例
        avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
        ret = avcodec_open2(video_decode_ctx, video_codec, nullptr); // 打开解码器的实例
        if (ret < 0) {
            LOGE("Can't open video_decode_ctx.\n");
            av_strerror(ret, errbuf, sizeof(errbuf));
            saveYUVInfo = "Can't open video_decode_ctx:" + to_string(ret) + "\n error msg：" +
                          string(errbuf) + "\n";
            PostStatusMessage(saveYUVInfo.c_str());
            return;
        }
    } else {
        LOGE("Can't find video stream.\n");
        saveYUVInfo = "Can't find video stream. \n";
        PostStatusMessage(saveYUVInfo.c_str());
        return;
    }
    LOGE("processImageProcedure33 destPath %s.\n", mSrcPath);

    packet = av_packet_alloc(); // 分配一个数据包
    frame = av_frame_alloc(); // 分配一个数据帧
    while (av_read_frame(in_fmt_ctx, packet) >= 0) { // 轮询数据包
        if (packet->stream_index == video_index) { // 视频包需要重新编码
            ret = decode_video(packet, frame,save_index); // 对视频帧解码
            if (ret == 0) {
                break; // 只保存一幅图像就退出
            }
        }
        av_packet_unref(packet); // 清除数据包
    }
    LOGI("Success save %d_index frame as yuv file.\n", save_index);
    saveYUVInfo = "Success save %d_index frame as yuv file.\n";
    PostStatusMessage(saveYUVInfo.c_str());
    av_frame_free(&frame); // 释放数据帧资源
    av_packet_free(&packet); // 释放数据包资源
    avcodec_close(video_decode_ctx); // 关闭视频解码器的实例
    avcodec_free_context(&video_decode_ctx); // 释放视频解码器的实例
    avformat_close_input(&in_fmt_ctx); // 关闭音视频文件
}

// 把视频帧保存为YUV图像。save_index表示要把第几个视频帧保存为图片
int SaveYUVFromVideo::save_yuv_file(AVFrame *frame, int save_index) {
    LOGE("save_yuv_file destPath %s.\n", mDestPath);

    // 视频帧的format字段为AVPixelFormat枚举类型，为0时表示AV_PIX_FMT_YUV420P
    LOGI("format = %d, width = %d, height = %d\n",
         frame->format, frame->width, frame->height);
    LOGI("target image file is %s\n", mDestPath);
    saveYUVInfo = "format =" + to_string(frame->format) + ",width =" + to_string(frame->width) +
                  ",height =" + to_string(frame->width) + "\n";
    saveYUVInfo = saveYUVInfo + "target image file is:" + string(mDestPath);
    PostStatusMessage(saveYUVInfo.c_str());

    FILE *fp = fopen(mDestPath, "wb"); // 以写方式打开文件
    if (!fp) {
        LOGE("open file %s fail.\n", mDestPath);
        return -1;
    }
    // 把YUV数据依次写入文件（按照YUV420P格式分解视频帧数据）
    int i = -1;
    while (++i < frame->height) { // 写入Y分量（灰度数值）
        fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
    }
    i = -1;
    while (++i < frame->height / 2) { // 写入U分量（色度数值的蓝色投影）
        fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
    }
    i = -1;
    while (++i < frame->height / 2) { // 写入V分量（色度数值的红色投影）
        fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
    }

    fclose(fp); // 关闭文件
    return 0;
}

// 对视频帧解码。save_index表示要把第几个视频帧保存为图片
int SaveYUVFromVideo::decode_video(AVPacket *packet, AVFrame *frame, int save_index) {
    LOGE("decode_video destPath %s.\n", mDestPath);

    // 把未解压的数据包发给解码器实例
    int ret = avcodec_send_packet(video_decode_ctx, packet);
    if (ret < 0) {
        LOGE("send packet occur error %d.\n", ret);
        return ret;
    }
    while (1) {
        // 从解码器实例获取还原后的数据帧
        ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            break;
        } else if (ret < 0) {
            LOGE("decode frame occur error %d.\n", ret);
            break;
        }
        packet_index++;
        if (packet_index < save_index) { // 还没找到对应序号的帧
            return AVERROR(EAGAIN);
        }
        save_yuv_file(frame, save_index); // 把视频帧保存为YUV图像
        break;
    }
    return ret;
}

JNIEnv *SaveYUVFromVideo::GetJNIEnv(bool *isAttach) {
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

void SaveYUVFromVideo::PostStatusMessage(const char *msg) {
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







