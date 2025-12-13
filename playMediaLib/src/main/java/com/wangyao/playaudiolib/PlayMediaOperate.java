package com.wangyao.playaudiolib;

import android.media.AudioFormat;
import android.media.AudioManager;
import android.media.AudioTrack;
import android.util.Log;
import android.view.Surface;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/11/10
 * Descibe : FFmpegPractice com.wangyao.playaudiolib
 */
public class PlayMediaOperate {
    private static final String TAG = PlayMediaOperate.class.getSimpleName();
    private AudioTrack mAudioTrack; // 播放PCM音频的音轨

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("playmedialib");
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

    public void playAudioByOpenSL(String audioPath) {
        native_play_audio_by_opensl(audioPath);
    }

    public void stopAudioByOpenSL() {
        native_stop_audio_by_opensl();
    }


    public void initVideoBySurface(String videoPath, Surface surface) {
        native_init_video_by_surface(videoPath, surface);
    }

    public void playVideoBySurface() {
        native_play_video_by_surface();
    }

    public void stopVideoBySurface() {
        native_stop_video_by_surface();
    }

    public void unInitVideoBySurface() {
        native_uninit_video_by_surface();
    }


    public void initVideoByGL(String videoPath, String fragPath, String vertexPath, Surface surface) {
        native_init_video_by_gl(videoPath, fragPath, vertexPath, surface);
    }

    public void playVideoByGL() {
        native_play_video_by_gl();
    }

    public void stopVideoByGL() {
        native_stop_video_by_gl();
    }

    public void unInitVideoByGL() {
        native_uninit_video_by_gl();
    }

    public void initMediaBySurface(String videoPath, Surface surface) {
        native_init_media_by_surface(videoPath, surface);
    }

    public void playMediaBySurface() {
        native_play_media_by_surface();
    }

    public void stopMediaBySurface() {
        native_stop_media_by_surface();
    }

    public void unInitMediaBySurface() {
        native_uninit_media_by_surface();
    }

    private native String native_string_from_jni();

    private native void native_play_audio_by_track(String audioPath);

    private native void native_stop_audio_by_track();

    private native void native_play_audio_by_opensl(String audioPath);

    private native void native_stop_audio_by_opensl();

    //surfaceView
    private native void native_init_video_by_surface(String videoPath, Surface surface);

    private native void native_play_video_by_surface();

    private native void native_stop_video_by_surface();

    private native void native_uninit_video_by_surface();

    //GL
    private native void native_init_video_by_gl(String videoPath, String fragPath, String vertexPath, Surface surface);

    private native void native_play_video_by_gl();

    private native void native_stop_video_by_gl();

    private native void native_uninit_video_by_gl();

    //surfaceView
    private native void native_init_media_by_surface(String videoPath, Surface surface);

    private native void native_play_media_by_surface();

    private native void native_stop_media_by_surface();

    private native void native_uninit_media_by_surface();


    private void CppStatusCallback(String status) {
        Log.e(TAG, "CppStatusCallback: " + status);
        if (mOnStatusMsgListener != null) {
            mOnStatusMsgListener.onStatusMsg(status);
        }
    }

    private void cppAudioFormatCallback(int sampleRate, int channelCount) {
        Log.d(TAG, "create sampleRate=" + sampleRate + ", channelCount=" + channelCount);
        int channelType = (channelCount == 1) ? AudioFormat.CHANNEL_OUT_MONO
                : AudioFormat.CHANNEL_OUT_STEREO;
        // 根据定义好的几个配置，来获取合适的缓冲大小
        int bufferSize = AudioTrack.getMinBufferSize(sampleRate,
                channelType, AudioFormat.ENCODING_PCM_16BIT);
        Log.d(TAG, "bufferSize=" + bufferSize);
        // 根据音频配置和缓冲区构建原始音频播放实例
        mAudioTrack = new AudioTrack(AudioManager.STREAM_MUSIC,
                sampleRate, channelType, AudioFormat.ENCODING_PCM_16BIT,
                bufferSize, AudioTrack.MODE_STREAM);
        mAudioTrack.play(); // 开始播放原始音频
        Log.d(TAG, "end create");
    }

    private void cppAudioBytesCallback(byte[] bytes, int size) {
        Log.d(TAG, "cppAudioBytesCallback size=" + size);
        if (mAudioTrack != null &&
                mAudioTrack.getPlayState() == AudioTrack.PLAYSTATE_PLAYING) {
            mAudioTrack.write(bytes, 0, size); // 将数据写入到音轨AudioTrack
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
