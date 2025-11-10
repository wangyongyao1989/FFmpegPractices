package com.wangyao.playaudiolib;

import android.util.Log;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/11/10
 * Descibe : FFmpegPractice com.wangyao.playaudiolib
 */
public class PlayAudioOperate {
    private static final String TAG = PlayAudioOperate.class.getSimpleName();


    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("playaudiolib");
    }

    public String stringFromC() {
        return native_string_from_jni();
    }

    private native String native_string_from_jni();


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
