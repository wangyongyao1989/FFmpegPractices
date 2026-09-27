package com.wangyao.rtsplib;

import android.util.Log;
import android.view.Surface;

/**
 * @author wangyongyao
 * @package com.wangyao.rtsplib
 * @date 2026/9/26
 * @decribe RTSP 拉流播放器 native 接口：支持 FFmpeg 软解 / MediaCodec 硬解切换
 * @project FFmpegPractices
 */
public class RtspOperate {
    private static final String TAG = RtspOperate.class.getSimpleName();

    // Used to load the 'rtsp' library on application startup.
    static {
        System.loadLibrary("rtsp");
    }

    public String getFFmpegVersion() {
        return native_get_rtsp_version();
    }

    /**
     * 开始拉流播放
     *
     * @param url        rtsp:// 地址
     * @param surface    SurfaceView 的渲染 Surface
     * @param hardDecode true=MediaCodec 硬解，false=FFmpeg 软解
     */
    public void playRtsp(String url, Surface surface, boolean hardDecode) {
        native_rtsp_play(url, surface, hardDecode);
    }

    public void stopRtsp() {
        native_rtsp_stop();
    }

    private native String native_get_rtsp_version();

    private native void native_rtsp_play(String url, Surface surface, boolean hardDecode);

    private native void native_rtsp_stop();

    private void CppStatusCallback(String status) {
        Log.e(TAG, "CppStatusCallback: " + status);
        if (mOnStatusMsgListener != null) {
            mOnStatusMsgListener.onStatusMsg(status);
        }
    }

    public interface OnStatusMsgListener {
        void onStatusMsg(String msg);
    }

    private OnStatusMsgListener mOnStatusMsgListener;

    public void setOnStatusMsgListener(OnStatusMsgListener listener) {
        this.mOnStatusMsgListener = listener;
    }
}
