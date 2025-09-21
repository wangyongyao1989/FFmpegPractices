package com.wangyao.hwcodeclib;

import android.util.Log;

/**
 * @author wangyongyao
 * @package com.wangyao.hwcodeclib
 * @date 2025/9/20 21:04
 * @decribe TODO
 * @project
 */
public class ProcessHwCodec {
    private static final String TAG = ProcessHwCodec.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("hwcodec");
    }

    public String stringFromC() {
        return native_hw_codec_string_from_jni();
    }

    public void processHwExtractor(String inputPath, String outputPath) {
        native_process_hw_extractor(inputPath, outputPath);
    }

    private native String native_hw_codec_string_from_jni();

    private native void native_process_hw_extractor(String inputPath, String outputPath);


    private void CppStatusCallback(String status) {
        Log.i(TAG, "CppStatusCallback: " + status);
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
