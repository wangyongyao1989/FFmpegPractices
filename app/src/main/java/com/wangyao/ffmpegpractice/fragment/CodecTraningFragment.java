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

import com.wangyao.codectraninglib.CodecOperate;
import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentCodecTraningLayoutBinding;
import com.wangyongyao.commonlib.utils.CommonFileUtils;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/8/12 17:27
 * Descibe : FFmpegPractice com.wangyao.ffmpegpractice.fragment
 */
public class CodecTraningFragment extends BaseFragment {

    private FFViewModel mFfViewModel;
    private FragmentCodecTraningLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtn1;
    private Button mBtn2;
    private Button mBtn3;

    private String mVideoPath1;

    private Button mBtCodecBack;
    private CodecOperate mCodecOperate;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentCodecTraningLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.sampleText;
        mBtCodecBack = mBinding.btnCodecBack;
        mBtn1 = mBinding.btnCodec1;
        mBtn2 = mBinding.btnCodec2;
        mBtn3 = mBinding.btnCodec3;

    }

    @Override
    public void initData() {
        mCodecOperate = new CodecOperate();
        mVideoPath1 = CommonFileUtils.getModelFilePath(getContext()
                , "video.mp4");

    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity())
                .get(FFViewModel.class);
    }

    @Override
    public void initListener() {
        mBtCodecBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mCodecOperate.getMediaMsg(mVideoPath1));
        });

        mBtn2.setOnClickListener(view -> {
            mTv.setText(mCodecOperate.getMediaTimeBase(mVideoPath1));
        });

        mBtn3.setOnClickListener(view -> {
            mTv.setText(mCodecOperate.getMediaTimeStamp(mVideoPath1));
        });

    }
}
