//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/13.
//

#include "includes/FFmpegOpenSLPlayer.h"
#include <unistd.h>

FFmpegOpenSLPlayer::FFmpegOpenSLPlayer()
        : mEngineObj(nullptr), mEngine(nullptr), mOutputMixObj(nullptr), mPlayerObj(nullptr),
          mPlayer(nullptr), mBufferQueue(nullptr), mFormatContext(nullptr), mCodecContext(nullptr),
          mSwrContext(nullptr), mAudioStreamIndex(-1), mSampleRate(44100), mChannels(2),
          mDuration(0), mSampleFormat(AV_SAMPLE_FMT_NONE), mIsPlaying(false), mInitialized(false),
          mStopRequested(false), mCurrentBuffer(0), mCurrentPosition(0), mBufferFull(false) {

    pthread_mutex_init(&mMutex, nullptr);
    pthread_cond_init(&mBufferReadyCond, nullptr);

    // 初始化缓冲区
    for (int i = 0; i < NUM_BUFFERS; i++) {
        mBuffers[i] = new uint8_t[BUFFER_SIZE];
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
}

bool FFmpegOpenSLPlayer::init(const std::string &filePath) {
    if (mInitialized) {
        LOGI("Already initialized");
        return true;
    }

    // 初始化 FFmpeg
    if (!initFFmpeg(filePath)) {
        LOGE("Failed to initialize FFmpeg");
        return false;
    }

    // 初始化 OpenSL ES
    if (!initOpenSL()) {
        LOGE("Failed to initialize OpenSL ES");
        cleanupFFmpeg();
        return false;
    }

    mInitialized = true;
    LOGI("FFmpegOpenSLPlayer initialized successfully");
    return true;
}

bool FFmpegOpenSLPlayer::initFFmpeg(const std::string &filePath) {
    // 注册所有编解码器
//    av_register_all();

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

    // 初始化重采样器（如果需要）
    mSwrContext = swr_alloc();
    av_opt_set_int(mSwrContext, "in_channel_count", mCodecContext->channels, 0);
    av_opt_set_int(mSwrContext, "out_channel_count", mChannels, 0);
    av_opt_set_int(mSwrContext, "in_sample_rate", mCodecContext->sample_rate, 0);
    av_opt_set_int(mSwrContext, "out_sample_rate", mSampleRate, 0);
    av_opt_set_sample_fmt(mSwrContext, "in_sample_fmt", mSampleFormat, 0);
    av_opt_set_sample_fmt(mSwrContext, "out_sample_fmt", AV_SAMPLE_FMT_S16, 0);

    if (swr_init(mSwrContext) < 0) {
        LOGE("Could not initialize resampler");
        return false;
    }

    LOGI("FFmpeg initialized: %d Hz, %d channels, duration: %lld",
         mSampleRate, mChannels, mDuration);
    return true;
}

bool FFmpegOpenSLPlayer::initOpenSL() {
    SLresult result;

    // 创建引擎
    result = slCreateEngine(&mEngineObj, 0, nullptr, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create engine: %d", result);
        return false;
    }

    // 实现引擎对象
    result = (*mEngineObj)->Realize(mEngineObj, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize engine: %d", result);
        return false;
    }

    // 获取引擎接口
    result = (*mEngineObj)->GetInterface(mEngineObj, SL_IID_ENGINE, &mEngine);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get engine interface: %d", result);
        return false;
    }

    // 创建输出混合器
    result = (*mEngine)->CreateOutputMix(mEngine, &mOutputMixObj, 0, nullptr, nullptr);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create output mix: %d", result);
        return false;
    }

    // 实现输出混合器
    result = (*mOutputMixObj)->Realize(mOutputMixObj, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize output mix: %d", result);
        return false;
    }

    // 配置音频源
    SLDataLocator_AndroidSimpleBufferQueue loc_bufq = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE,
            NUM_BUFFERS
    };

    SLDataFormat_PCM format_pcm = {
            SL_DATAFORMAT_PCM,
            static_cast<SLuint32>(mChannels),
            static_cast<SLuint32>(mSampleRate * 1000), // 转换为毫赫兹
            SL_PCMSAMPLEFORMAT_FIXED_16,
            SL_PCMSAMPLEFORMAT_FIXED_16,
            mChannels == 2 ? (SL_SPEAKER_FRONT_LEFT | SL_SPEAKER_FRONT_RIGHT)
                           : SL_SPEAKER_FRONT_CENTER,
            SL_BYTEORDER_LITTLEENDIAN
    };

    SLDataSource audioSrc = {&loc_bufq, &format_pcm};

    // 配置音频接收器
    SLDataLocator_OutputMix loc_outmix = {
            SL_DATALOCATOR_OUTPUTMIX,
            mOutputMixObj
    };
    SLDataSink audioSnk = {&loc_outmix, nullptr};

    // 创建音频播放器
    const SLInterfaceID ids[] = {SL_IID_BUFFERQUEUE};
    const SLboolean req[] = {SL_BOOLEAN_TRUE};

    result = (*mEngine)->CreateAudioPlayer(mEngine, &mPlayerObj, &audioSrc, &audioSnk,
                                           sizeof(ids) / sizeof(ids[0]), ids, req);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to create audio player: %d", result);
        return false;
    }

    // 实现播放器对象
    result = (*mPlayerObj)->Realize(mPlayerObj, SL_BOOLEAN_FALSE);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to realize player: %d", result);
        return false;
    }

    // 获取播放接口
    result = (*mPlayerObj)->GetInterface(mPlayerObj, SL_IID_PLAY, &mPlayer);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get play interface: %d", result);
        return false;
    }

    // 获取缓冲区队列接口
    result = (*mPlayerObj)->GetInterface(mPlayerObj, SL_IID_BUFFERQUEUE, &mBufferQueue);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to get buffer queue interface: %d", result);
        return false;
    }

    // 注册回调
    result = (*mBufferQueue)->RegisterCallback(mBufferQueue, bufferQueueCallback, this);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to register callback: %d", result);
        return false;
    }

    LOGI("OpenSL ES initialized successfully");
    return true;
}

bool FFmpegOpenSLPlayer::start() {
    if (!mInitialized) {
        LOGE("Player not initialized");
        return false;
    }

    pthread_mutex_lock(&mMutex);

    if (mIsPlaying) {
        pthread_mutex_unlock(&mMutex);
        return true;
    }

    // 设置播放状态
    SLresult result = (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_PLAYING);
    if (result != SL_RESULT_SUCCESS) {
        LOGE("Failed to set play state: %d", result);
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    mIsPlaying = true;
    mStopRequested = false;
    mBufferFull = false;

    // 启动解码线程
    if (pthread_create(&mDecodeThread, nullptr, decodeThreadWrapper, this) != 0) {
        LOGE("Failed to create decode thread");
        mIsPlaying = false;
        pthread_mutex_unlock(&mMutex);
        return false;
    }

    pthread_mutex_unlock(&mMutex);

    LOGI("Playback started");
    return true;
}

bool FFmpegOpenSLPlayer::pause() {
    if (!mInitialized || !mIsPlaying) {
        return false;
    }

    pthread_mutex_lock(&mMutex);

    SLresult result = (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_PAUSED);
    if (result == SL_RESULT_SUCCESS) {
        mIsPlaying = false;
        LOGI("Playback paused");
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

    if (mPlayer) {
        (*mPlayer)->SetPlayState(mPlayer, SL_PLAYSTATE_STOPPED);
    }

    // 清空缓冲区队列
    if (mBufferQueue) {
        (*mBufferQueue)->Clear(mBufferQueue);
    }

    LOGI("Playback stopped");
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

    while (!mStopRequested && mIsPlaying) {
        pthread_mutex_lock(&mMutex);

        // 如果缓冲区已满，等待信号
        while (mBufferFull && !mStopRequested && mIsPlaying) {
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
            ret = avcodec_receive_frame(mCodecContext, frame);
            if (ret == 0) {
                // 更新当前位置
                if (frame->pts != AV_NOPTS_VALUE) {
                    mCurrentPosition = frame->pts;
                }

                // 重采样音频数据
                uint8_t *buffer = mBuffers[mCurrentBuffer];
                uint8_t *outBuffer = buffer;
                int outSamples = swr_convert(mSwrContext, &outBuffer,
                                             BUFFER_SIZE / (mChannels * 2),
                                             (const uint8_t **) frame->data,
                                             frame->nb_samples);

                if (outSamples > 0) {
                    int bytesDecoded = outSamples * mChannels * 2;

                    // 将缓冲区加入播放队列
                    SLresult result = (*mBufferQueue)->Enqueue(mBufferQueue, buffer, bytesDecoded);
                    if (result != SL_RESULT_SUCCESS) {
                        LOGE("Failed to enqueue buffer: %d", result);
                    } else {
                        // 切换到下一个缓冲区
                        mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
                        mBufferFull = (mCurrentBuffer == 0); // 简单判断缓冲区是否满
                    }
                } else {
                    LOGE("swr_convert failed: %d", outSamples);
                }
            } else if (ret != AVERROR(EAGAIN)) {
                LOGE("Error receiving frame from decoder: %d", ret);
            }
        }

        av_packet_unref(&packet);
        pthread_mutex_unlock(&mMutex);
    }

    av_frame_free(&frame);
    LOGI("Decode thread finished");
}

void FFmpegOpenSLPlayer::bufferQueueCallback(SLAndroidSimpleBufferQueueItf bq, void *context) {
    FFmpegOpenSLPlayer *player = static_cast<FFmpegOpenSLPlayer *>(context);
    player->processBufferQueue();
}

void FFmpegOpenSLPlayer::processBufferQueue() {
    pthread_mutex_lock(&mMutex);

    // 有缓冲区播放完成，可以继续解码
    mBufferFull = false;
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
    if (mPlayerObj) {
        (*mPlayerObj)->Destroy(mPlayerObj);
        mPlayerObj = nullptr;
        mPlayer = nullptr;
        mBufferQueue = nullptr;
    }

    if (mOutputMixObj) {
        (*mOutputMixObj)->Destroy(mOutputMixObj);
        mOutputMixObj = nullptr;
    }

    if (mEngineObj) {
        (*mEngineObj)->Destroy(mEngineObj);
        mEngineObj = nullptr;
        mEngine = nullptr;
    }

    mIsPlaying = false;
}