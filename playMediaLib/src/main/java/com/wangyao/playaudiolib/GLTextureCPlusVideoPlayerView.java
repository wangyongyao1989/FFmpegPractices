package com.wangyao.playaudiolib;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.util.AttributeSet;
import android.util.Log;
import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

/**
 *
 */
public class GLTextureCPlusVideoPlayerView extends GLSurfaceView implements GLSurfaceView.Renderer {
    private static String TAG = GLTextureCPlusVideoPlayerView.class.getSimpleName();
    private PlayMediaOperate mJniCall;
    private Context mContext;

    private int mWidth;
    private int mHeight;

    public GLTextureCPlusVideoPlayerView(Context context
            , PlayMediaOperate jniCall) {
        super(context);
        mContext = context;
        mJniCall = jniCall;
        init();
    }

    public GLTextureCPlusVideoPlayerView(Context context, AttributeSet attrs) {
        super(context, attrs);
        mContext = context;
        init();
    }

    private void init() {
        getHolder().addCallback(this);
        setEGLContextClientVersion(3);
        setEGLConfigChooser(8, 8, 8, 8, 16, 0);
        setRenderer(this);
        setRenderMode(GLSurfaceView.RENDERMODE_WHEN_DIRTY);

    }


    public void onDrawFrame(GL10 gl) {
//        Log.e(TAG, "onDrawFrame: " + Thread.currentThread().getName());
//        if (mJniCall != null) {
//            mJniCall.glTextureVideoPlayRender();
//        }

    }

    public void onSurfaceChanged(GL10 gl, int width, int height) {
//        if (mJniCall != null) {
//            mJniCall.glTextureVideoPlayInit(null, null, width, height);
//        }
//        mWidth = width;
//        mHeight = height;

    }


    @Override
    public void onSurfaceCreated(GL10 gl10, EGLConfig eglConfig) {
        Log.e(TAG, "onSurfaceCreated:");


    }

    public void destroyRender() {
//        mJniCall.glTextureVideoPlayDestroy();
//        stopCameraPreview();
    }

}
