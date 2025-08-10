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



    private native String native_string_from_jni();

    private native boolean native_3d_animation_init_opengl(int width, int height);
}
