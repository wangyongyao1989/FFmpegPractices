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

    public void processHwMuxer(String inputPath, String outputPath1
            , String outputPath2, String fmt) {
        native_process_hw_muxer(inputPath, outputPath1, outputPath2, fmt);
    }

    public void mediaTransMuxer(String inputPath, String outputPath) {
        native_media_trans_muxer(inputPath, outputPath);
    }

    public void mediaExtractorDecodec(String inputPath) {
        native_media_extractor_decodec(inputPath);
    }

    public void processHwDeCodec(String inputPath, String outputPath1
            , String outputPath2, String codecName) {
        native_process_hw_decodec(inputPath, outputPath1, outputPath2, codecName);
    }

    public void processHwEnCodec(String inputPath, String outputPath1
            , String outputPath2, String codecName) {
        native_process_hw_encodec(inputPath, outputPath1, outputPath2, codecName);
    }

    private native String native_hw_codec_string_from_jni();

    private native void native_process_hw_extractor(String inputPath, String outputPath);

    private native void native_process_hw_muxer(String inputPath, String outputPath1
            , String outputPath2, String fmt);

    private native void native_process_hw_decodec(String inputPath, String outputPath1
            , String outputPath2, String codecName);

    private native void native_process_hw_encodec(String inputPath, String outputPath1
            , String outputPath2, String codecName);

    private native void native_media_trans_muxer(String inputPath, String outputPath);

    private native void native_media_extractor_decodec(String inputPath);


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
