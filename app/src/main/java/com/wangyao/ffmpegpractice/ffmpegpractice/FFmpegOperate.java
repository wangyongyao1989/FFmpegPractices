package com.wangyao.ffmpegpractice.ffmpegpractice;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.ffmpegpractice
 * @date 2025/8/10 10:31
 * @decribe TODO
 * @project
 */
public class FFmpegOperate {
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



    private native String native_string_from_jni();

    private native String native_get_ffmpeg_version();

}
