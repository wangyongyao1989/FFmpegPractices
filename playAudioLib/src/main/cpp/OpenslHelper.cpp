// Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/11.
//

#include "includes/OpenslHelper.h"

// 是否成功
bool OpenslHelper::isSuccess(SLresult &result) {
    return result == SL_RESULT_SUCCESS;
}

// 创建OpenSL引擎与引擎接口
SLresult OpenslHelper::createEngine() {
    // 创建引擎
    result = slCreateEngine(&engine, 0, NULL, 0, NULL, NULL);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化引擎，第二个参数为：是否异步
    result = (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取引擎接口
    result = (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineItf);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

// 创建混音器与混音接口
SLresult OpenslHelper::createMix() {
    // 获取混音器
    result = (*engineItf)->CreateOutputMix(engineItf, &mix, 0, 0, 0);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化混音器
    result = (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取环境混响混音器接口
    SLresult envResult = (*mix)->GetInterface(mix, SL_IID_ENVIRONMENTALREVERB, &envItf);
    if (isSuccess(envResult)) {
        // 给混音器设置环境
        (*envItf)->SetEnvironmentalReverbProperties(envItf, &settings);
    }

    return result;
}

// 创建播放器与播放接口
SLresult OpenslHelper::createPlayer(int numChannels, long samplesRate, int bitsPerSample, int channelMask) {
    // 关联音频流缓冲区。设为2是防止延迟，可以在播放另一个缓冲区时填充新数据
    SLDataLocator_AndroidSimpleBufferQueue buffQueque = {
            SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 2
    };

    // 缓冲区格式
    SLDataFormat_PCM dataFormat_pcm = {
            SL_DATAFORMAT_PCM,
            (SLuint32) numChannels,
            (SLuint32) samplesRate,
            (SLuint32) bitsPerSample,
            (SLuint32) bitsPerSample,
            (SLuint32) channelMask,
            SL_BYTEORDER_LITTLEENDIAN
    };

    // 数据源
    SLDataSource audioSrc = {&buffQueque, &dataFormat_pcm};

    // 关联混音器
    SLDataLocator_OutputMix dataLocator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSink = {&dataLocator_outputMix, NULL};

    // 通过引擎接口创建播放器
    SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    SLboolean required[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};

    // 创建音频播放器
    result = (*engineItf)->CreateAudioPlayer(
            engineItf, &player, &audioSrc, &audioSink, 3, ids, required);
    if (!isSuccess(result)) {
        return result;
    }

    // 实例化播放器
    result = (*player)->Realize(player, SL_BOOLEAN_FALSE);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取播放接口
    result = (*player)->GetInterface(player, SL_IID_PLAY, &playItf);
    if (!isSuccess(result)) {
        return result;
    }

    // 获取音量接口
    result = (*player)->GetInterface(player, SL_IID_VOLUME, &volumeItf);
    if (!isSuccess(result)) {
        return result;
    }

    // 注册缓冲区
    result = (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &bufferQueueItf);
    if (!isSuccess(result)) {
        return result;
    }

    return result;
}

// 注册回调入口
SLresult OpenslHelper::registerCallback(slAndroidSimpleBufferQueueCallback callback) {
    result = (*bufferQueueItf)->RegisterCallback(bufferQueueItf, callback, nullptr);
    return result;
}

// 开始播放
SLresult OpenslHelper::play() {
    playState = SL_PLAYSTATE_PLAYING;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PLAYING);
    return result;
}

// 暂停播放
SLresult OpenslHelper::pause() {
    playState = SL_PLAYSTATE_PAUSED;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_PAUSED);
    return result;
}

// 停止播放
SLresult OpenslHelper::stop() {
    playState = SL_PLAYSTATE_STOPPED;
    result = (*playItf)->SetPlayState(playItf, SL_PLAYSTATE_STOPPED);
    return result;
}

// 清理资源
void OpenslHelper::release() {
    if (player != nullptr) {
        (*player)->Destroy(player);
        player = nullptr;
        playItf = nullptr;
        bufferQueueItf = nullptr;
        volumeItf = nullptr;
    }
    if (mix != nullptr) {
        (*mix)->Destroy(mix);
        mix = nullptr;
        envItf = nullptr;
    }
    if (engine != nullptr) {
        (*engine)->Destroy(engine);
        engine = nullptr;
        engineItf = nullptr;
    }
}

// 析构
OpenslHelper::~OpenslHelper() {
    release();
}