package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.util.Log;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProvider;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentMainLayoutBinding;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/8/12 17:02
 * Descibe : FFmpegPractice com.wangyao.ffmpegpractice.fragment
 */
public class MainFragment extends BaseFragment {

    private static final String TAG = MainFragment.class.getSimpleName();
    private Button mBtnBasicTraining;
    private FFViewModel mFfViewModel;
    private FragmentMainLayoutBinding mBinding;
    private Button mBtnCodecTraining;
    private Button mBtnProcessImage;
    private Button mBtnProcessAudio;
    private Button mBtnProcessFilter;
    private Button mBtnProcessHwCodec;
    private Button mBtnPlayAudio;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentMainLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mBtnBasicTraining = mBinding.btnBasicTraining;
        mBtnCodecTraining = mBinding.btnCodecTraining;
        mBtnProcessImage = mBinding.btnProcessImage;
        mBtnProcessAudio = mBinding.btnProcessAudio;
        mBtnProcessFilter = mBinding.btnProcessFilter;
        mBtnProcessHwCodec = mBinding.btnProcessHwCodec;
        mBtnPlayAudio = mBinding.btnPlayAudio;

    }

    @Override
    public void initData() {

    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity())
                .get(FFViewModel.class);
    }

    @Override
    public void initListener() {
        mBtnBasicTraining.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.BASIC_TRANING);
        });

        mBtnCodecTraining.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.CODEC_TRANING);
        });

        mBtnProcessImage.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.PROCESS_IMAGE);
        });

        mBtnProcessAudio.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.PROCESS_AUDIO);
        });

        mBtnProcessFilter.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.PROCESS_FILTER);
        });

        mBtnProcessHwCodec.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.PROCESS_HW_CODEC);
        });

        mBtnPlayAudio.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.PLAY_AUDIO);
        });

    }
}