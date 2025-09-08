package com.wangyao.processfilterlib;

import android.util.Log;

/**
 * @author wangyongyao
 * @package com.wangyao.processfilterlib
 * @date 2025/9/7 11:44
 * @decribe TODO
 * @project
 */
public class ProcessFilterOperate {
    private static final String TAG = ProcessFilterOperate.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("processfilter");
    }

    public String getFFmpegVersion() {
        return native_get_filter_ffmpeg_version();

    }

    public void processVideoFilter(String srcPath, String outputPath, String filterCmd) {
        native_process_video_filter(srcPath, outputPath, filterCmd);
    }

    public void processVideoToPNG(String srcPath, String outputPath, String filterCmd) {
        native_process_video_to_png(srcPath, outputPath, filterCmd);
    }


    private native String native_get_filter_ffmpeg_version();

    private native void native_process_video_filter(String srcPath, String destPath
            , String filterCmd);

    private native void native_process_video_to_png(String srcPath, String destPath
            , String filterCmd);


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
