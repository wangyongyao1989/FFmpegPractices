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

    private native String native_string_from_jni();


}
