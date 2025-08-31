package com.wangyao.processimagelib;

import android.util.Log;

/**
 * @author wangyongyao
 * @package com.wangyao.processimagelib
 * @date 2025/8/31 18:46
 * @decribe TODO
 * @project
 */
public class ProcessImageOperate {
    private static final String TAG = ProcessImageOperate.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("processimage");
    }

    public String getFFmpegVersion() {
        return native_string_from_jni();
    }

    public void writeYUV(String destPath) {
        native_write_yuv(destPath);
    }


    private native String native_string_from_jni();

    private native void native_write_yuv(String destPath);


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
