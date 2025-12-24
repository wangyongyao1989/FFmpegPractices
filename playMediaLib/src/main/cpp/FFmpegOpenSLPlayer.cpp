//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/13.
//

#include "includes/FFmpegOpenSLPlayer.h"
#include <unistd.h>

FFmpegOpenSLPlayer::FFmpegOpenSLPlayer(JNIEnv *env, jobject thiz)
        : mEnv(nullptr), mJavaObj(nullptr), mFormatContext(nullptr), mCodecContext(nullptr),
          mSwrContext(nullptr),
          mAudioStreamIndex(-1), mSampleRate(44100), mChannels(2), mDuration(0),
          mSampleFormat(AV_SAMPLE_FMT_NONE), mIsPlaying(false), mInitialized(false),
          mStopRequested(false), mCurrentBuffer(0), mCurrentPosition(0),
          mQueuedBufferCount(0) {  // 初始化为0

    mEnv = env;
    env->GetJavaVM(&mJavaVm);
    mJavaObj = env->NewGlobalRef(thiz);

    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mBufferReadyCond, nullptr);

    // 初始化缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        mBuffers[i] = new uint8_t[BUFFER_SIZE];
        memset(mBuffers[i], 0, BUFFER_SIZE); // 初始化为静音
    }
}

FFmpegOpenSLPlayer::~FFmpegOpenSLPlayer() {
    stop();
    cleanup();

    pthread_mutex_destroy(&mMutex);
    pthread_cond_destroy(&mBufferReadyCond);

    // 清理缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        delete[] mBuffers[i];
    }
    mEnv->DeleteGlobalRef(mJavaObj);
}

bool FFmpegOpenSLPlayer::init(const std::string &filePath) {
    if (mInitialized) {
        LOGI("Already initialized");
        return true;
    }

    // 初始化 FFmpeg
    if (!initFFmpeg(filePath)) {
        LOGE("Failed to initialize FFmpeg");
        PostStatusMessage("Failed to initialize FFmpeg");
        return false;
    }

    // 初始化 OpenSL ES
    if (!initOpenSL()) {
        LOGE("Failed to initialize OpenSL ES");
        PostStatusMessage("Failed to initialize OpenSL ES");
        cleanupFFmpeg();
        return false;
    }

    mInitialized = true;
    LOGI("FFmpegOpenSLPlayer initialized successfully");
    PostStatusMessage("FFmpegOpenSLPlayer initialized successfully");
    return true;
}

bool FFmpegOpenSLPlayer::initFFmpeg(const std::string &filePath) {
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

    // 查找音频流
    mAudioStreamIndex = -1;
    for (unsigned int i = 0; i < mFormatContext->nb_streams; i++) {
        if (mFormatContext->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
            mAudioStreamIndex = i;
            break;
        }
    }

    if (mAudioStreamIndex == -1) {
        LOGE("Could not find audio stream");
        return false;
    }

    // 获取编解码器参数
    AVCodecParameters *codecParams = mFormatContext->streams[mAudioStreamIndex]->codecpar;

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

    // 设置音频参数
    mSampleRate = mCodecContext->sample_rate;
    mChannels = mCodecContext->channels;
    mSampleFormat = mCodecContext->sample_fmt;
    mDuration = mFormatContext->duration;

    // 配置音频重采样
    AVChannelLayout out_ch_layout = AV_CHANNEL_LAYOUT_STEREO;
    swr_alloc_set_opts2(&mSwrContext,
                        &out_ch_layout, AV_SAMPLE_FMT_S16, mCodecContext->sample_rate,
                        &mCodecContext->ch_layout, mCodecContext->sample_fmt, mCodecContext->sample_rate,
                        0, NULL);

    if (swr_init(mSwrContext) < 0) {
        LOGE("Could not initialize resampler");
        PostStatusMessage("Could not initialize resampler \n");
        return false;
    }

    LOGI("FFmpeg initialized: %d Hz, %d channels, duration: %lld",
         mSampleRate, mChannels, mDuration);
    playAudioInfo = "FFmpeg initialized ,Hz:" + to_string(mSampleRate) + ",channels:" +
                    to_string(mChannels) + " ,duration:" + to_string(mDuration) + "\n";
    PostStatusMessage(playAudioInfo.c_str());

    return true;
}

bool FFmpegOpenSLPlayer::initOpenSL() {
    // 创建 OpenSL 引擎与引擎接口
    SLresult result = helper.createEngine();
    if (!helper.isSuccess(result)) {
        LOGE("create engine error: %d", result);
        PostStatusMessage("Create engine error\n");
        return false;
    }
    PostStatusMessage("OpenSL createEngine Success \n");

    // 创建混音器与混音接口
    result = helper.createMix();
    if (!helper.isSuccess(result)) {
        LOGE("create mix error: %d", result);
        PostStatusMessage("Create mix error \n");
        return false;
    }
    PostStatusMessage("OpenSL createMix Success \n");

    result = helper.createPlayer(mChannels, mSampleRate * 1000, SL_PCMSAMPLEFORMAT_FIXED_16,
                                 mChannels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
                                                : SL_SPEAKER_FRONT_CENTER);
    if (!helper.isSuccess(result)) {
        LOGE("create player error: %d", result);
        PostStatusMessage("Create player error\n");
        return false;
    }
    PostStatusMessage("OpenSL createPlayer Success \n");

    // 注册回调并开始播放
    result = helper.registerCallback(bufferQueueCallback, this);
    if (!helper.isSuccess(result)) {
        LOGE("register callback error: %d", result);
        PostStatusMessage("Register callback error \n");
        return false;
    }
    PostStatusMessage("OpenSL registerCallback Success \n");

    // 清空缓冲区队列
    result = (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to clear buffer queue: %d, error: %s", result, getSLErrorString(result));
        playAudioInfo = "Failed to clear buffer queue:" + string(getSLErrorString(result));
        PostStatusMessage(playAudioInfo.c_str());
        return false;
    }

    LOGI("OpenSL ES initialized successfully: %d Hz, %d channels", mSampleRate, mChannels);
    playAudioInfo = "OpenSL ES  initialized ,Hz:" + to_string(mSampleRate) + ",channels:" +
                    to_string(mChannels) + " ,duration:" + to_string(mDuration) + "\n";
    PostStatusMessage(playAudioInfo.c_str());
    return true;
}

bool FFmpegOpenSLPlayer::start() {
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
    mQueuedBufferCount = 0;
    mCurrentBuffer = 0;
    mStopRequested = false;

    // 清空缓冲区队列
    if (helper.bufferQueueItf) {
        (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    }

    // 设置播放状态
    SLresult result = helper.play();
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to set play state: %d, error: %s", result, getSLErrorString(result));
        playAudioInfo = "Failed to set play state: " + string(getSLErrorString(result));
        PostStatusMessage(playAudioInfo.c_str());
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    mIsPlaying = true;

    // 启动解码线程
    if (pthread_create(&mDecodeThread, nullptr, decodeThreadWrapper, this) != 0) {
        LOGE("Failed to create decode thread");
        PostStatusMessage("Failed to create decode thread \n");
        mIsPlaying = false;
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    pthread_mutex_unlock(&mMutex);

    LOGI("Playback started");
    PostStatusMessage("Playback started \n");
    return true;
}

bool FFmpegOpenSLPlayer::pause() {
    if (!mInitialized || !mIsPlaying) {
        return false;
    }

    pthread_mutex_lock(&mMutex);

    SLresult result = helper.pause();
    if (result == SL_RESULT_SUCCESS) {
        mIsPlaying = false;
        LOGI("Playback paused");
        PostStatusMessage("Playback paused \n");
    } else {
        LOGE("Failed to pause playback: %d", result);
    }

    pthread_mutex_unlock(&mMutex);
    return result == SL_RESULT_SUCCESS;
}

void FFmpegOpenSLPlayer::stop() {
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

    if (helper.player) {
        helper.stop();
    }

    // 清空缓冲区队列
    if (helper.bufferQueueItf) {
        (*helper.bufferQueueItf)->Clear(helper.bufferQueueItf);
    }

    LOGI("Playback stopped");
    PostStatusMessage("Playback stopped \n");
}

void *FFmpegOpenSLPlayer::decodeThreadWrapper(void *context) {
    FFmpegOpenSLPlayer *player = static_cast<FFmpegOpenSLPlayer *>(context);
    player->decodeThread();
    return nullptr;
}

void FFmpegOpenSLPlayer::decodeThread() {
    AVPacket packet;
    AVFrame *frame = av_frame_alloc();
    int ret;

    LOGI("Decode thread started");
    PostStatusMessage("Decode thread started \n");

    while (!mStopRequested && mIsPlaying) {
        pthread_mutex_lock(&mMutex);

        // 等待直到有可用的缓冲区槽位
        while (mQueuedBufferCount >= NUM_BUFFERS && !mStopRequested && mIsPlaying) {
            LOGI("Waiting for buffer slot, queued: %d", mQueuedBufferCount.load());
            playAudioInfo =
                    "Waiting for buffer slot, queued:" + to_string(mQueuedBufferCount) + " \n";
            PostStatusMessage(playAudioInfo.c_str());
            pthread_cond_wait(&mBufferReadyCond, &mMutex);
        }

        if (mStopRequested || !mIsPlaying) {
            pthread_mutex_unlock(&mMutex);
            break;
        }

        // 读取音频包
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

        // 只处理音频包
        if (packet.stream_index == mAudioStreamIndex) {
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
                // 更新当前位置
                if (frame->pts != AV_NOPTS_VALUE) {
                    mCurrentPosition = frame->pts;
                }

                // 重采样音频数据
                uint8_t *buffer = mBuffers[mCurrentBuffer];
                uint8_t *outBuffer = buffer;

                // 计算最大输出样本数
                int maxSamples = BUFFER_SIZE / (mChannels * 2);
                int outSamples = swr_convert(mSwrContext, &outBuffer, maxSamples,
                                             (const uint8_t **) frame->data, frame->nb_samples);

                if (outSamples > 0) {
                    int bytesDecoded = outSamples * mChannels * 2;

                    // 确保不超过缓冲区大小
                    if (bytesDecoded > BUFFER_SIZE) {
                        LOGW("Decoded data exceeds buffer size: %d > %d", bytesDecoded,
                             BUFFER_SIZE);
                        bytesDecoded = BUFFER_SIZE;
                    }

                    // 检查是否还有可用的缓冲区槽位
                    if (mQueuedBufferCount >= NUM_BUFFERS) {
                        LOGW("No buffer slots available, skipping frame");
                        break;
                    }
                    LOGE("(*helper.bufferQueueItf)->Enqueue(: ");

                    // 将缓冲区加入播放队列
                    SLresult result = (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf,
                                                                        buffer, bytesDecoded);

                    if (result != SL_RESULT_SUCCESS) {
                        LOGE("Failed to enqueue buffer: %d, error: %s",
                             result, getSLErrorString(result));

                        if (result == SL_RESULT_BUFFER_INSUFFICIENT) {
                            // 等待一段时间后重试
                            pthread_mutex_unlock(&mMutex);
                            usleep(10000); // 10ms
                            pthread_mutex_lock(&mMutex);
                        }
                        break;
                    } else {
                        // 成功入队，更新状态
                        mQueuedBufferCount++;
                        mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
                        LOGI("Buffer enqueued successfully: %d bytes, buffer index: %d, queued: %d",
                             bytesDecoded, mCurrentBuffer, mQueuedBufferCount.load());
                    }
                } else if (outSamples < 0) {
                    LOGE("swr_convert failed: %d", outSamples);
                }
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

void FFmpegOpenSLPlayer::bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    FFmpegOpenSLPlayer *player = static_cast<FFmpegOpenSLPlayer *>(context);
    LOGI("bufferQueueCallback p=====================");

    player->processBufferQueue();
}

void FFmpegOpenSLPlayer::processBufferQueue() {
    pthread_mutex_lock(&mMutex);

    // 缓冲区已播放完成，减少计数
    if (mQueuedBufferCount > 0) {
        mQueuedBufferCount--;
        LOGI("Buffer processed, queued count: %d", mQueuedBufferCount.load());
    }

    // 通知解码线程有可用的缓冲区槽位
    pthread_cond_signal(&mBufferReadyCond);

    pthread_mutex_unlock(&mMutex);
}

void FFmpegOpenSLPlayer::cleanup() {
    cleanupFFmpeg();
    cleanupOpenSL();
}

void FFmpegOpenSLPlayer::cleanupFFmpeg() {
    if (mSwrContext) {
        swr_free(&mSwrContext);
        mSwrContext = nullptr;
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

    mAudioStreamIndex = -1;
    mInitialized = false;
}

void FFmpegOpenSLPlayer::cleanupOpenSL() {
    mIsPlaying = false;
}

// 添加这个辅助函数来获取错误描述
const char *FFmpegOpenSLPlayer::getSLErrorString(SLresult result) {
    switch (result) {
        case SL_RESULT_SUCCESS:
            return "SL_RESULT_SUCCESS";
        case SL_RESULT_PRECONDITIONS_VIOLATED:
            return "SL_RESULT_PRECONDITIONS_VIOLATED";
        case SL_RESULT_PARAMETER_INVALID:
            return "SL_RESULT_PARAMETER_INVALID";
        case SL_RESULT_MEMORY_FAILURE:
            return "SL_RESULT_MEMORY_FAILURE";
        case SL_RESULT_RESOURCE_ERROR:
            return "SL_RESULT_RESOURCE_ERROR";
        case SL_RESULT_RESOURCE_LOST:
            return "SL_RESULT_RESOURCE_LOST";
        case SL_RESULT_IO_ERROR:
            return "SL_RESULT_IO_ERROR";
        case SL_RESULT_BUFFER_INSUFFICIENT:
            return "SL_RESULT_BUFFER_INSUFFICIENT";
        case SL_RESULT_CONTENT_CORRUPTED:
            return "SL_RESULT_CONTENT_CORRUPTED";
        case SL_RESULT_CONTENT_UNSUPPORTED:
            return "SL_RESULT_CONTENT_UNSUPPORTED";
        case SL_RESULT_CONTENT_NOT_FOUND:
            return "SL_RESULT_CONTENT_NOT_FOUND";
        case SL_RESULT_PERMISSION_DENIED:
            return "SL_RESULT_PERMISSION_DENIED";
        case SL_RESULT_FEATURE_UNSUPPORTED:
            return "SL_RESULT_FEATURE_UNSUPPORTED";
        case SL_RESULT_INTERNAL_ERROR:
            return "SL_RESULT_INTERNAL_ERROR";
        case SL_RESULT_UNKNOWN_ERROR:
            return "SL_RESULT_UNKNOWN_ERROR";
        case SL_RESULT_OPERATION_ABORTED:
            return "SL_RESULT_OPERATION_ABORTED";
        case SL_RESULT_CONTROL_LOST:
            return "SL_RESULT_CONTROL_LOST";
        default:
            return "Unknown error";
    }
}

JNIEnv *FFmpegOpenSLPlayer::GetJNIEnv(bool *isAttach) {
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

void FFmpegOpenSLPlayer::PostStatusMessage(const char *msg) {
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