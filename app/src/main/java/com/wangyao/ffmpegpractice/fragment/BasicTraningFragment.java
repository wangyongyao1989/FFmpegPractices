package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentBasicTraningLayoutBinding;
import com.wangyongyao.basictraninglib.FFmpegOperate;
import com.wangyongyao.commonlib.utils.CommonFileUtils;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/8/12 17:27
 * Descibe : FFmpegPractice com.wangyao.ffmpegpractice.fragment
 */
public class BasicTraningFragment extends BaseFragment {

    private FFViewModel mFfViewModel;
    private FragmentBasicTraningLayoutBinding mBinding;
    private TextView mTv;
    private FFmpegOperate mFFmpegOperate;
    private Button mBtn1;
    private Button mBtn2;
    private Button mBtn3;

    private String mVideoPath1;
    private String mVideoPath2;

    private Button mBtnBsBack;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentBasicTraningLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.sampleText;
        mBtnBsBack = mBinding.btnBsBack;
        mBtn1 = mBinding.btnBs1;
        mBtn2 = mBinding.btnBs2;
        mBtn3 = mBinding.btnBs3;

    }

    @Override
    public void initData() {
        mFFmpegOperate = new FFmpegOperate();
        mTv.setText(mFFmpegOperate.stringFromC());
        mVideoPath1 = CommonFileUtils.getModelFilePath(getContext()
                , "video.mp4");
        mVideoPath2 = CommonFileUtils.getModelFilePath(getContext()
                , "woman.mp4");
    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity())
                .get(FFViewModel.class);
    }

    @Override
    public void initListener() {
        mBtnBsBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mFFmpegOperate.getFFmpegVersion() + "");
        });

        mBtn2.setOnClickListener(view -> {
            mTv.setText(mFFmpegOperate.getVideoMsg(mVideoPath1));
        });

        mBtn3.setOnClickListener(view -> {
            mTv.setText(mFFmpegOperate.getMediaMsg(mVideoPath2));
        });
    }
}
