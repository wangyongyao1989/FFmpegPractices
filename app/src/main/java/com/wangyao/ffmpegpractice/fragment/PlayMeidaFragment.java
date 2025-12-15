package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentPlayAudioLayoutBinding;
import com.wangyao.playaudiolib.PlayMediaOperate;
import com.wangyongyao.commonlib.utils.CommonFileUtils;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.fragment
 * @date 2025/11/10
 * @decribe TODO
 * @project
 */
public class PlayMeidaFragment extends BaseFragment {

    private FFViewModel mFfViewModel;
    private FragmentPlayAudioLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtn1;
    private Button mBtn2;
    private Button mBtn3;
    private Button mBtn4;
    private Button mBtn5;
    private Button mBtn6;

    private String mVideoPath1;
    private String mVideoPath2;

    private String mAACPath;
    private String mMp3Path;

    private String mH264Path;

    private String mWindowPngPath;
    private String mYaoJpgPath;

    private Button mBtCodecBack;
    private PlayMediaOperate mPlayMediaOperate;
    private StringBuilder mStringBuilder;

    private boolean isAudioTrackPlaying = false;

    private boolean isOpenSLPlaying = false;
    private boolean isSurfaceViewPlaying = false;
    private boolean isGLViewPlaying = false;
    private boolean isMediaPlaying = false;

    private SurfaceView mSurfaceView;
    private Surface mSurface;
    private String mFragPath;
    private String mVertexPath;
    private SurfaceView mGlPlayView;
    private Surface mGlSurface;
    private SurfaceView mFfMediaView;
    private Surface mMediaSurface;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentPlayAudioLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.tvPlayAudioMsg;
        mBtCodecBack = mBinding.btnPlayAudioBack;
        mBtn1 = mBinding.btnPlayAudio1;
        mBtn2 = mBinding.btnPlayAudio2;
        mBtn3 = mBinding.btnPlayAudio3;
        mBtn4 = mBinding.btnPlayVideo4;
        mBtn5 = mBinding.btnPlayVideo5;
        mBtn6 = mBinding.btnPlayVideo6;

        mSurfaceView = mBinding.surfacePlay;

        mGlPlayView = mBinding.glPlay;
        mFfMediaView = mBinding.ffMedia;

    }

    @Override
    public void initData() {
        mPlayMediaOperate = new PlayMediaOperate();
        mVideoPath1 = CommonFileUtils.getModelFilePath(getContext()
                , "video.mp4");
        mVideoPath2 = CommonFileUtils.getModelFilePath(getContext()
                , "midway.mp4");

        mAACPath = CommonFileUtils.getModelFilePath(getContext()
                , "fuzhous.aac");

        mMp3Path = CommonFileUtils.getModelFilePath(getContext()
                , "liudehua.mp3");

        mH264Path = CommonFileUtils.getModelFilePath(getContext()
                , "out.h264");

        mWindowPngPath = CommonFileUtils.getModelFilePath(getContext()
                , "window.png");

        mYaoJpgPath = CommonFileUtils.getModelFilePath(getContext()
                , "yao.jpg");

        mFragPath = CommonFileUtils.getModelFilePath(getContext()
                , "texture_video_play_frament.glsl");

        mVertexPath = CommonFileUtils.getModelFilePath(getContext()
                , "texture_video_play_vert.glsl");

        mStringBuilder = new StringBuilder();
    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity())
                .get(FFViewModel.class);
    }

    @Override
    public void initListener() {
        mPlayMediaOperate.setOnStatusMsgListener(msg -> {
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
            });

        });

        mBtCodecBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mPlayMediaOperate.stringFromC());
        });

        mBtn2.setOnClickListener(view -> {
            isAudioTrackPlaying = !isAudioTrackPlaying;
            if (isAudioTrackPlaying) {
                mPlayMediaOperate.playAudioByTrack(mAACPath);
                mBtn2.setText("AudioTrack停止");
            } else {
                mPlayMediaOperate.stopAudioByTrack();
                mBtn2.setText("AudioTrack播放");
            }
        });

        mBtn3.setOnClickListener(view -> {
            isOpenSLPlaying = !isOpenSLPlaying;
            if (isOpenSLPlaying) {
                mPlayMediaOperate.playAudioByOpenSL(mMp3Path);
                mBtn3.setText("OpenSL停止");
            } else {
                mPlayMediaOperate.stopAudioByOpenSL();
                mBtn3.setText("OpenSL播放");
            }
        });

        mBtn4.setOnClickListener(view -> {
            isSurfaceViewPlaying = !isSurfaceViewPlaying;
            if (isSurfaceViewPlaying) {
                mSurfaceView.setVisibility(View.VISIBLE);
                mPlayMediaOperate.playVideoBySurface();
                mBtn4.setText("SurfaceView停止");
            } else {
                mSurfaceView.setVisibility(View.GONE);
                mPlayMediaOperate.stopVideoBySurface();
                mBtn4.setText("SurfaceView播放");
            }
        });

        mBtn5.setOnClickListener(view -> {
            isGLViewPlaying = !isGLViewPlaying;
            if (isGLViewPlaying) {
                mGlPlayView.setVisibility(View.VISIBLE);
                mPlayMediaOperate.playVideoByGL();
                mBtn5.setText("GLTexture 停止");
            } else {
                mGlPlayView.setVisibility(View.GONE);
                mPlayMediaOperate.stopVideoByGL();
                mBtn5.setText("GLTexture 播放");
            }
        });

        mBtn6.setOnClickListener(view -> {
            isMediaPlaying = !isMediaPlaying;
            if (isMediaPlaying) {
                mFfMediaView.setVisibility(View.VISIBLE);
                mPlayMediaOperate.playMediaBySurface();
                mBtn5.setText("GLTexture 停止");
            } else {
                mFfMediaView.setVisibility(View.GONE);
                mPlayMediaOperate.stopMediaBySurface();
                mBtn5.setText("GLTexture 播放");
            }
        });

        final SurfaceHolder surfaceViewHolder = mSurfaceView.getHolder();
        surfaceViewHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                mSurface = holder.getSurface();
                mPlayMediaOperate.initVideoBySurface(mVideoPath1, mSurface);
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                mPlayMediaOperate.unInitVideoBySurface();
            }
        });

        final SurfaceHolder glViewHolder = mGlPlayView.getHolder();
        glViewHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                mGlSurface = holder.getSurface();
                mPlayMediaOperate.initVideoByGL(mVideoPath2, mFragPath, mVertexPath, mGlSurface);
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                mPlayMediaOperate.unInitVideoByGL();
            }
        });

        final SurfaceHolder mediaViewHolder = mFfMediaView.getHolder();
        mediaViewHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                mMediaSurface = holder.getSurface();
                mPlayMediaOperate.initMediaBySurface(mVideoPath2, mMediaSurface);
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format, int width, int height) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                mPlayMediaOperate.unInitMediaBySurface();
            }
        });
    }

}
