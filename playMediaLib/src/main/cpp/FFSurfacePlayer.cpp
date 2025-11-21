//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/20.
//

#include "includes/FFSurfacePlayer.h"
#include <unistd.h>

FFSurfacePlayer::FFSurfacePlayer(JNIEnv *env, jobject thiz)
        : mEnv(nullptr), mJavaObj(nullptr), mFormatContext(nullptr), mCodecContext(nullptr),
          mVideoStreamIndex(-1),
          mDuration(0),
          mSampleFormat(AV_SAMPLE_FMT_NONE),
          mIsPlaying(false),
          mInitialized(false),
          mStopRequested(false) {  // 初始化为0

    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mBufferReadyCond, nullptr);

}

FFSurfacePlayer::~FFSurfacePlayer() {
    stop();
    cleanup();

    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mBufferReadyCond);
    androidSurface = nullptr;
    mNativeWindow = nullptr;
    sws_freeContext(mSwsContext);
    av_frame_free(&mRgbFrame);
    videoFrameQueue.stop();
    mEnv->DeleteGlobalRef(mJavaObj);
}

bool FFSurfacePlayer::init(const std::string &filePath, jobject surface) {
    if (mInitialized) {
        LOGI("Already initialized");
        return true;
    }
    androidSurface = surface;
    // 初始化 FFmpeg
    if (!initFFmpeg(filePath)) {
        LOGE("Failed to initialize FFmpeg");
        PostStatusMessage("Failed to initialize FFmpeg");
        return false;
    }

    //初始化 ANativeWindow
    if (!initANativeWindow()) {
        LOGE("Failed to initialize OinitANativeWindow");
        PostStatusMessage("Failed to initialize initANativeWindow");
        return false;
    }

    mInitialized = true;
    LOGI("FFSurfacePlayer initialized successfully");
    PostStatusMessage("FFSurfacePlayer initialized successfully");
    return true;
}

bool FFSurfacePlayer::initFFmpeg(const std::string &filePath) {
    // 打开输入文件
    if (avformat_open_input(&mFormatContext, filePath.c_str(), nullptr, nullptr) != 0) {
        LOGE("Could not open file: %s", filePath.c_str());
        return false;
    }

    // 查找流信息
    if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
        LOGE("Could not find stream information");
        return false;
    }

    // 查找视频流
    mVideoStreamIndex = -1;
    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++) {
        if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoStreamIndex = i;
            break;
        }
    }

    if (mVideoStreamIndex == -1) {
        LOGE("Could not find audio stream");
        return false;
    }

    // 获取编解码器参数
    AVCodecParameters *codecParams = mFormatContext->streams[mVideoStreamIndex]->codecpar;

    // 查找解码器
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        LOGE("Unsupported codec");
        return false;
    }

    // 分配编解码器上下文
    mCodecContext = avcodec_alloc_context3(codec);
    if (!mCodecContext) {
        LOGE("Could not allocate codec context");
        return false;
    }

    // 复制参数到上下文
    if (avcodec_parameters_to_context(mCodecContext, codecParams) < 0) {
        LOGE("Could not copy codec parameters");
        return false;
    }

    // 打开编解码器
    if (avcodec_open2(mCodecContext, codec, nullptr) < 0) {
        LOGE("Could not open codec");
        return false;
    }

    // 设置视频参数
    mWidth = mCodecContext->width;
    mHeight = mCodecContext->height;
    mSampleFormat = mCodecContext->sample_fmt;
    mDuration = mFormatContext->duration;

    LOGI("FFmpeg initialized mWidth: %d ,-mHeight: %d , duration: %lld",
         mWidth, mHeight, mDuration);
    playMediaInfo = "FFmpeg initialized ,mWidth:" + to_string(mWidth) + ",mHeight:" +
                    to_string(mHeight) + " ,duration:" + to_string(mDuration) + "\n";
    PostStatusMessage(playMediaInfo.c_str());

    return true;
}


bool FFSurfacePlayer::initANativeWindow() {
    //6.初始化ANativeWindow
    mNativeWindow = ANativeWindow_fromSurface(mEnv, androidSurface);
    if (nullptr == mNativeWindow) {
        LOGD("Couldn't get native window from surface.\n");
        return false;
    }
    LOGD("获取到 native window from surface\n");
    mRgbFrame = av_frame_alloc();

    mWidth = mCodecContext->width;
    mHeight = mCodecContext->height;
    LOGD("width： %d，==height:%d", mWidth, mHeight);

    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mWidth, mHeight, 1);
    LOGD("计算解码后的rgb %d\n", bufferSize);

    mOutbuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));

    //   转换器
    mSwsContext = sws_getContext(mWidth, mHeight, mCodecContext->pix_fmt,
                                 mWidth, mHeight, AV_PIX_FMT_RGBA,
                                 SWS_BICUBIC, nullptr, nullptr,
                                 nullptr);
    int32_t i = ANativeWindow_setBuffersGeometry(mNativeWindow, mWidth, mHeight,
                                                 WINDOW_FORMAT_RGBA_8888);
    if (0 > i) {
        LOGD("Couldn't set buffers geometry.\n");
        ANativeWindow_release(mNativeWindow);
        return false;
    }

    av_image_fill_arrays(mRgbFrame->data, mRgbFrame->linesize,
                         mOutbuffer, AV_PIX_FMT_RGBA,
                         mWidth, mHeight, 1);

    LOGD("ANativeWindow_setBuffersGeometry成功\n");
    return true;
}


bool FFSurfacePlayer::start() {
    if (!mInitialized) {
        LOGE("Player not initialized");
        PostStatusMessage("Player not initialized \n");
        return false;
    }

    pthread_mutex_lock(&mMutex);

    if (mIsPlaying) {
        pthread_mutex_unlock(&mMutex);
        return true;
    }

    // 重置状态
    mStopRequested = false;
    mIsPlaying = true;

    // 启动解码线程
    if (pthread_create(&mDecodeThread, nullptr, decodeThreadWrapper, this) != 0) {
        LOGE("Failed to create decode thread");
        PostStatusMessage("Failed to create decode thread \n");
        mIsPlaying = false;
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    // 启动渲染线程
    if (pthread_create(&mRenderThread, nullptr, renderVideoFrame, this) != 0) {
        LOGE("Failed to create render thread");
        PostStatusMessage("Failed to create render thread \n");
        mIsPlaying = false;
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    pthread_mutex_unlock(&mMutex);

    LOGI("Playback started");
    PostStatusMessage("Playback started \n");
    return true;
}

bool FFSurfacePlayer::pause() {
    if (!mInitialized || !mIsPlaying) {
        return false;
    }

    pthread_mutex_lock(&mMutex);


    pthread_mutex_unlock(&mMutex);
    return true;
}

void FFSurfacePlayer::stop() {
    if (!mInitialized) {
        return;
    }

    mStopRequested = true;
    mIsPlaying = false;

    // 通知所有等待的线程
    pthread_cond_broadcast(&mBufferReadyCond);

    // 等待解码线程结束
    if (mDecodeThread) {
        pthread_join(mDecodeThread, nullptr);
        mDecodeThread = 0;
    }


    // 等待解码线程结束
    if (mRenderThread) {
        pthread_join(mRenderThread, nullptr);
        mRenderThread = 0;
    }

    LOGI("Playback stopped");
    PostStatusMessage("Playback stopped \n");
}

void *FFSurfacePlayer::decodeThreadWrapper(void *context) {
    FFSurfacePlayer *player = static_cast<FFSurfacePlayer *>(context);
    player->decodeThread();
    return nullptr;
}


void FFSurfacePlayer::decodeThread() {
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    int ret;

    LOGI("Decode thread started");
    PostStatusMessage("Decode thread started \n");

    while (!mStopRequested && mIsPlaying) {
        pthread_mutex_lock(&mMutex);
        // 等待直到有可用的缓冲区槽位
        while (videoFrameQueue.size() >= maxVideoFrames && !mStopRequested && mIsPlaying) {
            LOGI("Waiting for buffer slot, queued: %d", videoFrameQueue.size());
            playMediaInfo =
                    "Waiting for buffer slot, queued:" + to_string(videoFrameQueue.size()) + " \n";
            PostStatusMessage(playMediaInfo.c_str());
            pthread_cond_wait(&mBufferReadyCond, &mMutex);
        }

        if (mStopRequested || !mIsPlaying) {
            pthread_mutex_unlock(&mMutex);
            break;
        }

        // 读取视频包
        ret = av_read_frame(mFormatContext, &packet);
        if (ret < 0) {
            pthread_mutex_unlock(&mMutex);

            if (ret == AVERROR_EOF) {
                LOGI("End of file reached");
                // 可以选择循环播放或停止
                break;
            } else {
                LOGE("Error reading frame: %d", ret);
                usleep(10000); // 短暂休眠后继续
                continue;
            }
        }

        // 只处理视频包
        if (packet.stream_index == mVideoStreamIndex) {
            // 发送包到解码器
            ret = avcodec_send_packet(mCodecContext, &packet);
            if (ret < 0) {
                LOGE("Error sending packet to decoder: %d", ret);
                av_packet_unref(&packet);
                pthread_mutex_unlock(&mMutex);
                continue;
            }

            // 接收解码后的帧
            while (avcodec_receive_frame(mCodecContext, frame) == 0) {
                if (frame) {
                    // 创建视频帧副本
                    AVFrame *frameCopy = av_frame_alloc();
                    if (av_frame_ref(frameCopy, frame) > 0) {
                        videoFrameQueue.push(frameCopy);
                    }
                    av_frame_free(&frameCopy);
                }
                sendFrameDataToANativeWindow(frame);
            }
        }

        av_packet_unref(&packet);
        pthread_mutex_unlock(&mMutex);
        // 给其他线程一些执行时间
        usleep(1000);
    }

    av_frame_free(&frame);
    LOGI("Decode thread finished");
}


void *FFSurfacePlayer::renderVideoFrame(void *context) {
    FFSurfacePlayer *player = static_cast<FFSurfacePlayer *>(context);
    player->renderVideoThread();
    return nullptr;
}

void FFSurfacePlayer::renderVideoThread() {
    LOGI("render thread started");
    PostStatusMessage("render thread started \n");
    while (!mStopRequested && mIsPlaying) {
        if (videoFrameQueue.size() > 0) {
            shared_ptr<AVFrame *> avFrame = videoFrameQueue.pop();
            sendFrameDataToANativeWindow(*avFrame);
        }
    }
    LOGI("renderVideoThread finished");
}

int FFSurfacePlayer::sendFrameDataToANativeWindow(AVFrame *frame) {
    ANativeWindow_Buffer windowBuffer;
    // 未压缩的数据
    sws_scale(mSwsContext, frame->data,
              frame->linesize, 0,
              mCodecContext->height, mRgbFrame->data,
              mRgbFrame->linesize);
    auto lock = ANativeWindow_lock(mNativeWindow, &windowBuffer, nullptr);
    if (lock < 0) {
        LOGD("cannot lock window");
    } else {
        //将图像绘制到界面上，注意这里pFrameRGBA一行的像素和windowBuffer一行的像素长度可能不一致
        //需要转换好，否则可能花屏
        auto *dst = (uint8_t *) windowBuffer.bits;
        for (int h = 0; h < mHeight; h++) {
            memcpy(dst + h * windowBuffer.stride * 4,
                   mOutbuffer + h * mRgbFrame->linesize[0],
                   mRgbFrame->linesize[0]);
        }
    }

    av_usleep(33);
    ANativeWindow_unlockAndPost(mNativeWindow);
    if (videoFrameQueue.size() < maxVideoFrames / 3) {
        // 通知解码线程有可用的缓冲区槽位
        LOGI("缓冲区槽位小于：%d,通知解码线程继续解码", maxVideoFrames / 3);
        playMediaInfo =
                "缓冲区槽位小于:" + to_string(maxVideoFrames / 3) + "通知解码线程继续解码" +
                " \n";
        PostStatusMessage(playMediaInfo.c_str());
        pthread_cond_signal(&mBufferReadyCond);
    }
    return lock;
}


void FFSurfacePlayer::cleanup() {
    cleanupFFmpeg();
    mIsPlaying = false;
}

void FFSurfacePlayer::cleanupFFmpeg() {
    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }

    if (mCodecContext) {
        avcodec_close(mCodecContext);
        avcodec_free_context(&mCodecContext);
        mCodecContext = nullptr;
    }

    if (mFormatContext) {
        avformat_close_input(&mFormatContext);
        mFormatContext = nullptr;
    }

    mVideoStreamIndex = -1;
    mInitialized = false;
}


JNIEnv *FFSurfacePlayer::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    int status;
    if (nullptr == mJavaVm) {
        LOGE("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }
    *isAttach = false;
    status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGE("GetJNIEnv failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    }
    return env;
}

void FFSurfacePlayer::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) {
        return;
    }

    jmethodID mid = pEnv->GetMethodID(pEnv->GetObjectClass(mJavaObj), "CppStatusCallback",
                                      "(Ljava/lang/String;)V");
    if (mid) {
        jstring jMsg = pEnv->NewStringUTF(msg);
        pEnv->CallVoidMethod(mJavaObj, mid, jMsg);
        pEnv->DeleteLocalRef(jMsg);
    }

    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
}