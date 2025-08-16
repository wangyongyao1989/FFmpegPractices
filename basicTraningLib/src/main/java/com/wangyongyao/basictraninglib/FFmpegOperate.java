package com.wangyongyao.basictraninglib;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.ffmpegpractice
 * @date 2025/8/10 10:31
 * @decribe TODO
 * @project
 */
public class FFmpegOperate {
    private static final String TAG = FFmpegOperate.class.getSimpleName();

    // Used to load the 'ffmpegpractice' library on application startup.
    static {
        System.loadLibrary("ffmpegpractice");
    }

    public String stringFromC() {
        return native_string_from_jni();
    }

    public String getFFmpegVersion() {
        return native_get_ffmpeg_version();
    }

    public String getVideoMsg(String videPath) {
        return native_get_video_msg(videPath);
    }

    public String getMediaMsg(String videPath) {
        return native_get_media_msg(videPath);
    }

    public String getMediaCodecMsg(String videPath) {
        return native_get_media_codec_msg(videPath);
    }

    public String mediaCopyToDecodec(String videPath) {
        return native_media_copy_to_decodec(videPath);
    }

    public int writeMediaToMP4(String videPath) {
        return native_write_media_to_mp4(videPath);
    }

    public String writeMediaFilter(String videPath) {
        return native_write_media_filter(videPath);
    }

    private native String native_string_from_jni();

    private native String native_get_ffmpeg_version();

    private native String native_get_video_msg(String fragPath);

    private native String native_get_media_msg(String fragPath);

    private native String native_get_media_codec_msg(String fragPath);

    private native String native_media_copy_to_decodec(String fragPath);

    private native int native_write_media_to_mp4(String fragPath);

    private native String native_write_media_filter(String fragPath);

}
