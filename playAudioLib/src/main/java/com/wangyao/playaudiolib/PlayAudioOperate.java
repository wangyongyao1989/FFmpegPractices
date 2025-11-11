package com.wangyao.playaudiolib;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
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

    public void playAudioByTrack(String audioPath) {
        native_play_audio_by_track(audioPath);
    }

    public void stopAudioByTrack() {
        native_stop_audio_by_track();
    }

    private native String native_string_from_jni();

    private native void native_play_audio_by_track(String audioPath);

    private native void native_stop_audio_by_track();


    private void CppStatusCallback(String status) {
        Log.e(TAG, "CppStatusCallback: " + status);
        if (mOnStatusMsgListener != null) {
            mOnStatusMsgListener.onStatusMsg(status);
        }
    }

    private void cppAudioFormatCallback(int sampleRate, int channelCount) {
        Log.d(TAG, "create sampleRate=" + sampleRate + ", channelCount=" + channelCount);

    }

    private void cppAudioBytesCallback(byte[] bytes, int size) {
        Log.d(TAG, "cppAudioBytesCallback size=" + size);

    }

    public interface OnStatusMsgListener {
        void onStatusMsg(String msg);
    }

    private OnStatusMsgListener mOnStatusMsgListener;

    public void setOnStatusMsgListener(OnStatusMsgListener listener) {
        this.mOnStatusMsgListener = listener;
    }


}
