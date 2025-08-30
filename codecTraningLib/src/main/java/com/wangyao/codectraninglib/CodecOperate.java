package com.wangyao.codectraninglib;

import android.util.Log;

/**
 * @author wangyongyao
 * @package com.wangyao.codectraninglib
 * @date 2025/8/16 20:27
 * @decribe TODO
 * @project
 */
public class CodecOperate {
    private static final String TAG = CodecOperate.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("codectraning");
    }

    public String stringFromC() {
        return native_string_from_jni();
    }

    public String getMediaMsg(String mediaPath) {
        return native_get_media_msg(mediaPath);
    }

    public String getMediaTimeBase(String mediaPath) {
        return native_get_media_time_base(mediaPath);
    }

    public String getMediaTimeStamp(String mediaPath) {
        return native_get_media_time_stamp(mediaPath);
    }

    public String copyMediaFile(String srcPath, String destPath) {
        return native_copy_media_file(srcPath, destPath);
    }

    public String peelAudioOfMedia(String srcPath, String destPath) {
        return native_peel_audio_of_media(srcPath, destPath);
    }

    public String splitVideoOfMedia(String srcPath, String destPath) {
        return native_split_video_of_media(srcPath, destPath);
    }

    public String mergeAudio(String srcVideoPath, String srcAudioPath, String destPath) {
        return native_merge_audio(srcVideoPath, srcAudioPath, destPath);
    }

    public void recodeVideo(String srcVideoPath, String destPath) {
        native_recodec_video(srcVideoPath, destPath);
    }

    public void mergeVideo(String srcVideoPath1, String srcVideoPath2, String destPath) {
        native_merge_video(srcVideoPath1, srcVideoPath2, destPath);
    }


    private native String native_string_from_jni();

    private native String native_get_media_msg(String mediaPath);

    private native String native_get_media_time_base(String mediaPath);

    private native String native_get_media_time_stamp(String mediaPath);

    private native String native_copy_media_file(String srcPath, String destPath);

    private native String native_peel_audio_of_media(String srcPath, String destPath);

    private native String native_split_video_of_media(String srcPath, String destPath);

    private native String native_merge_audio(String srcVideoPath, String srcAudioPath, String destPath);

    private native void native_recodec_video(String srcPath, String destPath);

    private native void native_merge_video(String srcPath1, String srcPath2, String destPath);

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
