package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.LayoutInflater;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentRtspLayoutBinding;
import com.wangyao.rtsplib.RtspOperate;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.fragment
 * @date 2026/9/26
 * @decribe TODO RTSP 拉流播放器：软解（FFmpeg）/硬解（MediaCodec）切换
 * @project
 */
public class RtspFragment extends BaseFragment {

    private static final String DEFAULT_RTSP_URL = "rtsp://192.168.1.4:8554/live";

    private FFViewModel mFfViewModel;
    private FragmentRtspLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtnBack;
    private Button mBtnSoft;
    private Button mBtnHard;
    private Button mBtnStop;
    private Button mBtnVersion;
    private EditText mEtUrl;

    private SurfaceView mSurfaceView;
    private Surface mSurface;

    private RtspOperate mRtspOperate;
    private StringBuilder mStringBuilder;

    private boolean isPlaying = false;
    // SurfaceView 的 Surface 一旦被软解路径（ANativeWindow）连接占用过，
    // MediaCodec 再 configure 同一 Surface 会报 nativeWindowConnect -22 而启动失败。
    // 因此每次起播前先 GONE/VISIBLE 重建 Surface，surfaceCreated 后再真正播放。
    private boolean pendingPlay = false;
    private boolean pendingHard = false;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentRtspLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.tvRtspMsg;
        mBtnBack = mBinding.btnRtspBack;
        mBtnSoft = mBinding.btnRtspSoft;
        mBtnHard = mBinding.btnRtspHard;
        mBtnStop = mBinding.btnRtspStop;
        mBtnVersion = mBinding.btnRtspVersion;
        mEtUrl = mBinding.etRtspUrl;

        mSurfaceView = mBinding.surfaceRtspPlay;
    }

    @Override
    public void initData() {
        mRtspOperate = new RtspOperate();
        mEtUrl.setText(DEFAULT_RTSP_URL);
        mEtUrl.setSelection(DEFAULT_RTSP_URL.length());
        mStringBuilder = new StringBuilder();
    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity()).get(FFViewModel.class);
    }

    @Override
    public void initListener() {
        mRtspOperate.setOnStatusMsgListener(msg -> {
            if (getActivity() == null) {
                return;
            }
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
                if (msg.contains("会话结束") || msg.contains("已停止")) {
                    isPlaying = false;
                }
            });
        });

        mBtnBack.setOnClickListener(view -> {
            mRtspOperate.stopRtsp();
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtnVersion.setOnClickListener(view -> {
            mTv.setText(mRtspOperate.getFFmpegVersion());
        });

        mBtnSoft.setOnClickListener(view -> {
            if (isPlaying) {
                mRtspOperate.stopRtsp();
            }
            startPlay(false);
        });

        mBtnHard.setOnClickListener(view -> {
            if (isPlaying) {
                mRtspOperate.stopRtsp();
            }
            startPlay(true);
        });

        mBtnStop.setOnClickListener(view -> {
            mRtspOperate.stopRtsp();
            isPlaying = false;
        });

        final SurfaceHolder surfaceViewHolder = mSurfaceView.getHolder();
        surfaceViewHolder.addCallback(new SurfaceHolder.Callback() {
            @Override
            public void surfaceCreated(@NonNull SurfaceHolder holder) {
                mSurface = holder.getSurface();
                if (pendingPlay) {
                    pendingPlay = false;
                    doPlay(pendingHard);
                }
            }

            @Override
            public void surfaceChanged(@NonNull SurfaceHolder holder, int format
                    , int width, int height) {

            }

            @Override
            public void surfaceDestroyed(@NonNull SurfaceHolder holder) {
                mRtspOperate.stopRtsp();
                mSurface = null;
                isPlaying = false;
            }
        });
    }

    private void startPlay(boolean hardDecode) {
        String url = mEtUrl.getText().toString().trim();
        if (TextUtils.isEmpty(url)) {
            url = DEFAULT_RTSP_URL;
        }
        mStringBuilder.append(hardDecode ? ">>> 启动硬解拉流: " : ">>> 启动软解拉流: ")
                .append(url).append("\n");
        mTv.setText(mStringBuilder);

        // 无论上次是否用过，一律重建 Surface，保证每次起播拿到干净窗口
        pendingPlay = true;
        pendingHard = hardDecode;
        mSurfaceView.setVisibility(View.GONE);
        mSurfaceView.post(() -> mSurfaceView.setVisibility(View.VISIBLE));
        mSurfaceView.postDelayed(() -> {
            if (pendingPlay) {
                pendingPlay = false;
                mStringBuilder.append("Surface 重建超时，请重试\n");
                mTv.setText(mStringBuilder);
            }
        }, 3000);
    }

    private void doPlay(boolean hardDecode) {
        if (mSurface == null || !mSurface.isValid()) {
            mStringBuilder.append("Surface 尚未就绪，无法播放\n");
            mTv.setText(mStringBuilder);
            return;
        }
        mRtspOperate.playRtsp(mEtUrl.getText().toString().trim(), mSurface, hardDecode);
        isPlaying = true;
    }
}
