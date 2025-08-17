package com.wangyao.codectraninglib;

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


    private native String native_string_from_jni();

    private native String native_get_media_msg(String mediaPath);

    private native String native_get_media_time_base(String mediaPath);

    private native String native_get_media_time_stamp(String mediaPath);

    private native String native_copy_media_file(String srcPath, String destPath);

    private native String native_peel_audio_of_media(String srcPath, String destPath);

}
