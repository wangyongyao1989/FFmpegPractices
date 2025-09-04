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

    public void saveYUVFromVideo(String srcPath, String destPath) {
        native_save_yuv_from_video(srcPath, destPath);
    }

    public void saveJPGFromVideo(String srcPath, String destPath) {
        native_save_jpg_from_video(srcPath, destPath);
    }

    public void saveJPGSwsFromVideo(String srcPath, String destPath) {
        native_save_jpg_sws_from_video(srcPath, destPath);
    }

    public void savePNGSwsFromVideo(String srcPath, String destPath) {
        native_save_png_sws_from_video(srcPath, destPath);
    }

    public void saveBMPSwsFromVideo(String srcPath, String destPath) {
        native_save_bmp_sws_from_video(srcPath, destPath);
    }

    public void saveGIFFromVideo(String srcPath, String destPath) {
        native_save_gif_from_video(srcPath, destPath);
    }

    public void saveImage2Video(String srcPath1, String srcPath2, String destPath) {
        native_save_image_to_video(srcPath1, srcPath2,destPath);
    }

    private native String native_string_from_jni();

    private native void native_write_yuv(String destPath);

    private native void native_save_yuv_from_video(String srcPath, String destPath);

    private native void native_save_jpg_from_video(String srcPath, String destPath);

    private native void native_save_jpg_sws_from_video(String srcPath, String destPath);

    private native void native_save_png_sws_from_video(String srcPath, String destPath);

    private native void native_save_bmp_sws_from_video(String srcPath, String destPath);

    private native void native_save_gif_from_video(String srcPath, String destPath);

    private native void native_save_image_to_video(String srcPath1,String srcPath2,  String destPath);

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
