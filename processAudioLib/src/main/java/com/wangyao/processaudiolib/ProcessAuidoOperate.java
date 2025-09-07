package com.wangyao.processaudiolib;

import android.util.Log;

/**
 * @author wangyongyao
 * @package com.wangyao.processaudiolib
 * @date 2025/9/6 09:53
 * @decribe TODO
 * @project
 */
public class ProcessAuidoOperate {
    private static final String TAG = ProcessAuidoOperate.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("processaudio");
    }

    public String getFFmpegVersion() {
        return native_get_audio_ffmpeg_version();
    }

    public void savePCMOfMedia(String srcPath, String destPath) {
        native_save_pcm_of_media(srcPath, destPath);
    }

    public void saveAACOfMedia(String srcPath, String destPath) {
        native_save_aac_of_media(srcPath, destPath);
    }

    public void saveWAVOfMedia(String srcPath, String destPath1, String destPath2) {
        native_save_wav_of_media(srcPath, destPath1, destPath2);
    }


    private native String native_get_audio_ffmpeg_version();

    private native void native_save_pcm_of_media(String srcPath, String destPath);

    private native void native_save_aac_of_media(String srcPath, String destPath);

    private native void native_save_wav_of_media(String srcPath1, String destPath1, String destPath2);


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
