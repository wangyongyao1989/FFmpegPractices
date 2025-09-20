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
import com.wangyao.ffmpegpractice.databinding.FragmentHwCodecLayoutBinding;
import com.wangyao.hwcodeclib.ProcessHwCodec;
import com.wangyongyao.commonlib.utils.CommonFileUtils;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.fragment
 * @date 2025/9/20 20:58
 * @decribe TODO
 * @project
 */
public class ProcessHwCodecFragment extends BaseFragment {

    private FFViewModel mFfViewModel;
    private FragmentHwCodecLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtn1;

    private String mVideoPath1;
    private String mVideoPath2;

    private String mAACPath;

    private String mH264Path;

    private String mWindowPngPath;
    private String mYaoJpgPath;

    private Button mBtCodecBack;
    private ProcessHwCodec mProcessHwCodec;
    private StringBuilder mStringBuilder;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentHwCodecLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.tvProcessImage;
        mBtCodecBack = mBinding.btnHwCodecBack;
        mBtn1 = mBinding.btnHwCodec1;


    }

    @Override
    public void initData() {
        mProcessHwCodec = new ProcessHwCodec();
        mVideoPath1 = CommonFileUtils.getModelFilePath(getContext()
                , "video.mp4");
        mVideoPath2 = CommonFileUtils.getModelFilePath(getContext()
                , "midway.mp4");

        mAACPath = CommonFileUtils.getModelFilePath(getContext()
                , "fuzhous.aac");

        mH264Path = CommonFileUtils.getModelFilePath(getContext()
                , "out.h264");

        mWindowPngPath = CommonFileUtils.getModelFilePath(getContext()
                , "window.png");

        mYaoJpgPath = CommonFileUtils.getModelFilePath(getContext()
                , "yao.jpg");
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
        mProcessHwCodec.setOnStatusMsgListener(msg -> {
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
            });

        });

        mBtCodecBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mProcessHwCodec.stringFromC());
        });



    }

}
