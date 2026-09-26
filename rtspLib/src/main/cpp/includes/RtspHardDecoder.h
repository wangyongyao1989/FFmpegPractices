//  Author : wangyao https://github.com/wangyongyao1989
// RTSP 拉流播放器：AMediaCodec（NDK）硬解码封装
// 只负责“喂 AnnexB 包 → 出帧渲染到 ANativeWindow”，同步与流控由 RtspPlayer 调度

#ifndef FFMPEGPRACTICE_RTSP_HARD_DECODER_H
#define FFMPEGPRACTICE_RTSP_HARD_DECODER_H

#include <media/NdkMediaCodec.h>
#include <media/NdkMediaFormat.h>
#include <media/NdkMediaError.h>
#include <android/native_window.h>
#include "LogUtils.h"

class RtspHardDecoder {
public:
    RtspHardDecoder() = default;

    ~RtspHardDecoder() {
        release();
    }

    // 禁用拷贝
    RtspHardDecoder(const RtspHardDecoder &) = delete;

    RtspHardDecoder &operator=(const RtspHardDecoder &) = delete;

    /**
     * 创建解码器并绑定渲染窗口
     * @param mime     "video/avc" / "video/hevc"
     * @param sps/pps  AnnexB 格式参数集（含起始码），可为空
     */
    bool init(const char *mime, int width, int height,
              const uint8_t *sps, size_t spsSize,
              const uint8_t *pps, size_t ppsSize,
              ANativeWindow *window);

    ssize_t dequeueInput(size_t timeoutUs) {
        return AMediaCodec_dequeueInputBuffer(mCodec, timeoutUs);
    }

    bool queueData(const uint8_t *data, size_t size, int64_t ptsUs, uint32_t flags);

    ssize_t dequeueOutput(AMediaCodecBufferInfo *info, size_t timeoutUs) {
        return AMediaCodec_dequeueOutputBuffer(mCodec, info, timeoutUs);
    }

    bool releaseOutput(ssize_t index, bool render);

    // 处理 INFO_OUTPUT_FORMAT_CHANGED，取出解码分辨率
    bool onFormatChanged(ssize_t code, int &width, int &height);

    void flush() {
        if (mCodec) {
            AMediaCodec_flush(mCodec);
        }
    }

    void stop();

    void release();

    bool isReady() const { return mStarted; }

    static const ssize_t kFormatChanged = AMEDIACODEC_INFO_OUTPUT_FORMAT_CHANGED;
    static const ssize_t kTryAgainLater = AMEDIACODEC_INFO_TRY_AGAIN_LATER;

private:
    AMediaCodec *mCodec = nullptr;
    AMediaFormat *mFormat = nullptr;
    bool mStarted = false;
};

#endif //FFMPEGPRACTICE_RTSP_HARD_DECODER_H
