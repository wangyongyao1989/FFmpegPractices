// Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/20.

#include "includes/FFSurfacePlayer.h"
#include <unistd.h>

FFSurfacePlayer::FFSurfacePlayer(JNIEnv *env, jobject thiz)
        : mEnv(nullptr), mJavaObj(nullptr), mFormatContext(nullptr),
          mCodecContext(nullptr), mVideoStreamIndex(-1), mDuration(0),
          mSampleFormat(AV_SAMPLE_FMT_NONE), mWidth(0), mHeight(0),
          mIsPlaying(false), mInitialized(false), mStopRequested(false),
          mNativeWindow(nullptr), mSwsContext(nullptr), mRgbFrame(nullptr),
          mOutbuffer(nullptr), mDecodeThread(0), mRenderThread(0) {

    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

    pthread_mutex_init(&mDecodeMutex, nullptr);
    pthread_mutex_init(&mRenderMutex, nullptr);
    pthread_cond_init(&mBufferMaxCond, nullptr);
    pthread_cond_init(&mRenderCond, nullptr);
}

FFSurfacePlayer::~FFSurfacePlayer() {
    stop();
    cleanup();

    pthread_mutex_destroy(&mDecodeMutex);
    pthread_mutex_destroy(&mRenderMutex);
    pthread_cond_destroy(&mBufferMaxCond);
    pthread_cond_destroy(&mRenderCond);

    if (androidSurface) {
        mEnv->DeleteLocalRef(androidSurface);
    }
    if (mNativeWindow) {
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
    }
    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }
    if (mRgbFrame) {
        av_frame_free(&mRgbFrame);
        mRgbFrame = nullptr;
    }
    if (mOutbuffer) {
        av_free(mOutbuffer);
        mOutbuffer = nullptr;
    }

    mEnv->DeleteGlobalRef(mJavaObj);
}

bool FFSurfacePlayer::init(const std::string &filePath, jobject surface) {
    if (mInitialized) {
        LOGI("Already initialized");
        return true;
    }

    androidSurface = mEnv->NewGlobalRef(surface);

    if (!initFFmpeg(filePath)) {
        LOGE("Failed to initialize FFmpeg");
        PostStatusMessage("Failed to initialize FFmpeg");
        return false;
    }

    if (!initANativeWindow()) {
        LOGE("Failed to initialize ANativeWindow");
        PostStatusMessage("Failed to initialize ANativeWindow");
        cleanupFFmpeg();
        return false;
    }

    mInitialized = true;
    LOGI("FFSurfacePlayer initialized successfully");
    PostStatusMessage("FFSurfacePlayer initialized successfully");
    return true;
}

bool FFSurfacePlayer::initFFmpeg(const std::string &filePath) {
    if (avformat_open_input(&mFormatContext, filePath.c_str(), nullptr, nullptr) != 0) {
        LOGE("Could not open file: %s", filePath.c_str());
        return false;
    }

    if (avformat_find_stream_info(mFormatContext, nullptr) < 0) {
        LOGE("Could not find stream information");
        avformat_close_input(&mFormatContext);
        return false;
    }

    mVideoStreamIndex = -1;
    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++) {
        if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
            mVideoStreamIndex = i;
            break;
        }
    }

    if (mVideoStreamIndex == -1) {
        LOGE("Could not find video stream");
        avformat_close_input(&mFormatContext);
        return false;
    }

    AVCodecParameters *codecParams = mFormatContext->streams[mVideoStreamIndex]->codecpar;
    const AVCodec *codec = avcodec_find_decoder(codecParams->codec_id);
    if (!codec) {
        LOGE("Unsupported codec");
        avformat_close_input(&mFormatContext);
        return false;
    }

    mCodecContext = avcodec_alloc_context3(codec);
    if (!mCodecContext) {
        LOGE("Could not allocate codec context");
        avformat_close_input(&mFormatContext);
        return false;
    }

    if (avcodec_parameters_to_context(mCodecContext, codecParams) < 0) {
        LOGE("Could not copy codec parameters");
        cleanupFFmpeg();
        return false;
    }

    if (avcodec_open2(mCodecContext, codec, nullptr) < 0) {
        LOGE("Could not open codec");
        cleanupFFmpeg();
        return false;
    }

    mWidth = mCodecContext->width;
    mHeight = mCodecContext->height;
    mSampleFormat = mCodecContext->sample_fmt;
    mDuration = mFormatContext->duration;

    LOGI("FFmpeg initialized width: %d, height: %d, duration: %lld",
         mWidth, mHeight, mDuration);

    playMediaInfo = "FFmpeg initialized, width:" + std::to_string(mWidth) +
                    ", height:" + std::to_string(mHeight) +
                    ", duration:" + std::to_string(mDuration) + "\n";
    PostStatusMessage(playMediaInfo.c_str());

    return true;
}

bool FFSurfacePlayer::initANativeWindow() {
    mNativeWindow = ANativeWindow_fromSurface(mEnv, androidSurface);
    if (!mNativeWindow) {
        LOGE("Couldn't get native window from surface");
        return false;
    }

    mRgbFrame = av_frame_alloc();
    if (!mRgbFrame) {
        LOGE("Could not allocate RGB frame");
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    int bufferSize = av_image_get_buffer_size(AV_PIX_FMT_RGBA, mWidth, mHeight, 1);
    mOutbuffer = (uint8_t *) av_malloc(bufferSize * sizeof(uint8_t));
    if (!mOutbuffer) {
        LOGE("Could not allocate output buffer");
        av_frame_free(&mRgbFrame);
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    mSwsContext = sws_getContext(mWidth, mHeight, mCodecContext->pix_fmt,
                                 mWidth, mHeight, AV_PIX_FMT_RGBA,
                                 SWS_BICUBIC, nullptr, nullptr, nullptr);
    if (!mSwsContext) {
        LOGE("Could not create sws context");
        av_free(mOutbuffer);
        mOutbuffer = nullptr;
        av_frame_free(&mRgbFrame);
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
        return false;
    }

    if (ANativeWindow_setBuffersGeometry(mNativeWindow, mWidth, mHeight,
                                         WINDOW_FORMAT_RGBA_8888) < 0) {
        LOGE("Couldn't set buffers geometry");
        cleanupANativeWindow();
        return false;
    }

    if (av_image_fill_arrays(mRgbFrame->data, mRgbFrame->linesize,
                             mOutbuffer, AV_PIX_FMT_RGBA,
                             mWidth, mHeight, 1) < 0) {
        LOGE("Could not fill image arrays");
        cleanupANativeWindow();
        return false;
    }

    LOGI("ANativeWindow initialization successful");
    return true;
}

void FFSurfacePlayer::cleanupANativeWindow() {
    if (mSwsContext) {
        sws_freeContext(mSwsContext);
        mSwsContext = nullptr;
    }
    if (mRgbFrame) {
        av_frame_free(&mRgbFrame);
        mRgbFrame = nullptr;
    }
    if (mOutbuffer) {
        av_free(mOutbuffer);
        mOutbuffer = nullptr;
    }
    if (mNativeWindow) {
        ANativeWindow_release(mNativeWindow);
        mNativeWindow = nullptr;
    }
}

bool FFSurfacePlayer::start() {
    if (!mInitialized) {
        LOGE("Player not initialized");
        PostStatusMessage("Player not initialized \n");
        return false;
    }

    pthread_mutex_lock(&mDecodeMutex);

    if (mIsPlaying) {
        // 每个 return 都在 mDecodeMutex 保护区内，必须先解锁再返回。
        // 原先三处 return 都不解锁：解码/渲染线程一上来就要拿这把锁，
        // 于是永远拿不到，第二次播放直接卡死在这里。
        pthread_mutex_unlock(&mDecodeMutex);
        return true;
    }

    mStopRequested = false;
    mIsPlaying = true;
    // 不能用 ThreadSafeQueue::clear()：它只把元素 pop 掉，AVFrame* 指向的帧
    // 本体连同引用的解码器缓冲区全留在 native heap 上（它自己的注释就写着
    // 「如果 T 是指针类型，可能需要特殊处理」）。这里逐帧真正释放。
    // pop(T&) 在队列空且未 stop 时会阻塞，所以必须带 empty() 守卫。
    AVFrame *leftover = nullptr;
    while (!videoFrameQueue.empty() && videoFrameQueue.pop(leftover)) {
        if (leftover) {
            av_frame_unref(leftover);
            av_frame_free(&leftover);
        }
    }

    if (pthread_create(&mDecodeThread, nullptr, decodeThreadWrapper, this) != 0) {
        LOGE("Failed to create decode thread");
        PostStatusMessage("Failed to create decode thread");
        mIsPlaying = false;
        pthread_mutex_unlock(&mDecodeMutex);
        return false;
    }

    if (pthread_create(&mRenderThread, nullptr, renderVideoFrame, this) != 0) {
        LOGE("Failed to create render thread");
        PostStatusMessage("Failed to create render thread");
        mIsPlaying = false;
        mStopRequested = true;
        pthread_cond_signal(&mBufferMaxCond);
        pthread_join(mDecodeThread, nullptr);
        mDecodeThread = 0;
        // 必须在 join 之后解锁：解码线程正等这把锁，持锁 join 是死锁
        pthread_mutex_unlock(&mDecodeMutex);
        return false;
    }

    pthread_mutex_unlock(&mDecodeMutex);

    LOGI("Playback started");
    PostStatusMessage("Playback started");
    return true;
}

void FFSurfacePlayer::stop() {
    if (!mInitialized) {
        return;
    }

    mStopRequested = true;
    mIsPlaying = false;

    // 通知所有等待的线程
    pthread_cond_broadcast(&mBufferMaxCond);
    pthread_cond_broadcast(&mRenderCond);

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

    // 停止时把残余帧逐帧释放（同 start() 里的说明），clear() 只丢指针不释放帧。
    AVFrame *leftover = nullptr;
    while (!videoFrameQueue.empty() && videoFrameQueue.pop(leftover)) {
        if (leftover) {
            av_frame_unref(leftover);
            av_frame_free(&leftover);
        }
    }

    LOGI("Playback stopped");
    PostStatusMessage("Playback stopped");
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

    if (!frame) {
        LOGE("Could not allocate frame");
        return;
    }

    LOGI("Decode thread started");
    PostStatusMessage("Decode thread started");

    while (!mStopRequested && mIsPlaying) {
        pthread_mutex_lock(&mDecodeMutex);
        // 当队列达到最大时，解码等待。
        while (videoFrameQueue.size() >= maxVideoFrames && !mStopRequested && mIsPlaying) {
            LOGD("Waiting for buffer slot, queued: %zu", videoFrameQueue.size());
            playMediaInfo =
                    "Waiting for buffer slot, queued:" + to_string(videoFrameQueue.size()) + " \n";
            PostStatusMessage(playMediaInfo.c_str());
            pthread_cond_wait(&mBufferMaxCond, &mDecodeMutex);
        }

        if (mStopRequested || !mIsPlaying) {
            pthread_mutex_unlock(&mDecodeMutex);
            break;
        }

        ret = av_read_frame(mFormatContext, &packet);
        if (ret < 0) {
            pthread_mutex_unlock(&mDecodeMutex);

            if (ret == AVERROR_EOF) {
                LOGI("End of file reached");
                break;
            } else {
                LOGE("Error reading frame: %d", ret);
                usleep(10000);
                continue;
            }
        }

        if (packet.stream_index == mVideoStreamIndex) {
            ret = avcodec_send_packet(mCodecContext, &packet);
            if (ret < 0) {
                LOGE("Error sending packet to decoder: %d", ret);
                av_packet_unref(&packet);
                pthread_mutex_unlock(&mDecodeMutex);
                continue;
            }

            while (avcodec_receive_frame(mCodecContext, frame) == 0) {
                AVFrame *frameCopy = av_frame_alloc();
                if (!frameCopy) {
                    LOGE("Could not allocate frame copy");
                    continue;
                }
                if (av_frame_ref(frameCopy, frame) >= 0) {
                    if (!videoFrameQueue.push(frameCopy)) {
                        // 队列已 stop 时 push 返回 false 且不接管指针，
                        // 不接住这个返回值就等于每帧漏一帧
                        av_frame_unref(frameCopy);
                        av_frame_free(&frameCopy);
                    } else {
                        pthread_cond_signal(&mRenderCond);
                    }
                } else {
                    av_frame_free(&frameCopy);
                    // 这里不能 unlock：整个 while 循环都处在 mDecodeMutex 保护区内，
                    // 由循环末尾统一释放。原先在失败分支提前解锁，接着走到下面的
                    // pthread_mutex_unlock(&mDecodeMutex) 就是同一把锁解两次，
                    // 非递归锁二次解锁是未定义行为，之后所有加/解锁的配对都乱了。
                }
            }
        }

        av_packet_unref(&packet);
        pthread_mutex_unlock(&mDecodeMutex);
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
    LOGI("Render thread started");
    PostStatusMessage("Render thread started \n");

    AVRational timeBase = mFormatContext->streams[mVideoStreamIndex]->time_base;
    int64_t lastPts = AV_NOPTS_VALUE;

    while (!mStopRequested && mIsPlaying) {
        pthread_mutex_lock(&mRenderMutex);

        while (videoFrameQueue.empty() && !mStopRequested && mIsPlaying) {
            pthread_cond_wait(&mRenderCond, &mRenderMutex);
        }

        if (mStopRequested || !mIsPlaying) {
            pthread_mutex_unlock(&mRenderMutex);
            break;
        }

        if (!videoFrameQueue.empty()) {
            std::shared_ptr<AVFrame *> framePtr = videoFrameQueue.pop();
            AVFrame *frame = *framePtr;
            pthread_mutex_unlock(&mRenderMutex);

            // 基于时间戳的帧率控制
            if (lastPts != AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE) {
                int64_t ptsDiff = frame->pts - lastPts;
                double timeDiff = av_q2d(timeBase) * ptsDiff * 1000000; // 转换为微秒
                if (timeDiff > 0 && timeDiff < 1000000) { // 合理的帧间隔
                    usleep(static_cast<useconds_t>(timeDiff));
                }
            }
            lastPts = frame->pts;

            sendFrameDataToANativeWindow(frame);

            // 队列里存的是 av_frame_alloc()+av_frame_ref() 得到的引用，渲染完必须
            // 归还。原先只让包着指针的 shared_ptr<AVFrame*> 析构——它销毁的是那个
            // 「指针的指针」，AVFrame 本体和它引用的解码器缓冲区一个都没释放，
            // 于是每渲染一帧漏一帧（FFGLPlayer 的同名循环是有 av_frame_free 的）。
            av_frame_unref(frame);
            av_frame_free(&frame);

            // 通知解码线程
            if (videoFrameQueue.size() < maxVideoFrames / 2) {
                pthread_cond_signal(&mBufferMaxCond);
            }
        } else {
            pthread_mutex_unlock(&mRenderMutex);
        }
    }

    LOGI("Render thread finished");
}

int FFSurfacePlayer::sendFrameDataToANativeWindow(AVFrame *frame) {
    if (!mNativeWindow || !frame) {
        return -1;
    }

    ANativeWindow_Buffer windowBuffer;

    // 颜色空间转换
    sws_scale(mSwsContext, frame->data, frame->linesize, 0,
              mHeight, mRgbFrame->data, mRgbFrame->linesize);

    // 锁定窗口缓冲区
    if (ANativeWindow_lock(mNativeWindow, &windowBuffer, nullptr) < 0) {
        LOGE("Cannot lock window");
        return -1;
    }

    // 优化拷贝：检查 stride 是否匹配
    uint8_t* dst = static_cast<uint8_t*>(windowBuffer.bits);
    int dstStride = windowBuffer.stride * 4;  // 目标步长（字节）
    int srcStride = mRgbFrame->linesize[0];   // 源步长（字节）

    if (dstStride == srcStride) {
        // 步长匹配，可以直接整体拷贝
        memcpy(dst, mOutbuffer, srcStride * mHeight);
    } else {
        // 步长不匹配，需要逐行拷贝
        for (int h = 0; h < mHeight; h++) {
            memcpy(dst + h * dstStride,
                   mOutbuffer + h * srcStride,
                   srcStride);
        }
    }

    ANativeWindow_unlockAndPost(mNativeWindow);

    // 流控制
    if (videoFrameQueue.size() < maxVideoFrames / 2) {
        pthread_cond_signal(&mBufferMaxCond);
    }

    return 0;
}

void FFSurfacePlayer::cleanup() {
    cleanupFFmpeg();
    cleanupANativeWindow();
    mIsPlaying = false;
    mInitialized = false;
}

void FFSurfacePlayer::cleanupFFmpeg() {
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
}

JNIEnv *FFSurfacePlayer::GetJNIEnv(bool *isAttach) {
    if (!mJavaVm) {
        LOGE("GetJNIEnv mJavaVm == nullptr");
        return nullptr;
    }

    JNIEnv *env;
    int status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);

    if (status == JNI_EDETACHED) {
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) {
            LOGE("Failed to attach current thread");
            return nullptr;
        }
        *isAttach = true;
    } else if (status != JNI_OK) {
        LOGE("Failed to get JNIEnv");
        return nullptr;
    } else {
        *isAttach = false;
    }

    return env;
}

void FFSurfacePlayer::PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *env = GetJNIEnv(&isAttach);
    if (!env) {
        return;
    }

    jmethodID mid = env->GetMethodID(env->GetObjectClass(mJavaObj),
                                     "CppStatusCallback", "(Ljava/lang/String;)V");
    if (mid) {
        jstring jMsg = env->NewStringUTF(msg);
        env->CallVoidMethod(mJavaObj, mid, jMsg);
        env->DeleteLocalRef(jMsg);
    }

    if (isAttach) {
        mJavaVm->DetachCurrentThread();
    }
}