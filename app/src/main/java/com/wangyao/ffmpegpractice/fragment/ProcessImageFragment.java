package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.ScrollView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.databinding.FragmentProcessImageLayoutBinding;
import com.wangyao.processimagelib.ProcessImageOperate;
import com.wangyongyao.commonlib.utils.CommonFileUtils;
import com.wangyongyao.commonlib.utils.DirectoryPath;

import java.util.Random;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.fragment
 * @date 2025/8/31 18:58
 * @decribe TODO
 * @project
 */
public class ProcessImageFragment extends BaseFragment {
    private FFViewModel mFfViewModel;
    private FragmentProcessImageLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtn1;
    private Button mBtn2;

    private String mVideoPath1;
    private String mVideoPath2;

    private String mAACPath;

    private String mH264Path;

    private Button mBtCodecBack;
    private ProcessImageOperate mProcessImage;
    private StringBuilder mStringBuilder;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentProcessImageLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.tvProcessImage;
        mBtCodecBack = mBinding.btnCodecBack;
        mBtn1 = mBinding.btnProcessImage1;
        mBtn2 = mBinding.btnProcessImage2;


    }

    @Override
    public void initData() {
        mProcessImage = new ProcessImageOperate();
        mVideoPath1 = CommonFileUtils.getModelFilePath(getContext()
                , "video.mp4");
        mVideoPath2 = CommonFileUtils.getModelFilePath(getContext()
                , "midway.mp4");

        mAACPath = CommonFileUtils.getModelFilePath(getContext()
                , "fuzhous.aac");

        mH264Path = CommonFileUtils.getModelFilePath(getContext()
                , "out.h264");
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
        mProcessImage.setOnStatusMsgListener(msg -> {
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
            });

        });

        mBtCodecBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mProcessImage.getFFmpegVersion());
        });

        mBtn2.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1; // 加1是为了包括100
            String outputPath = videoDir + "write_yuv" + randomInt + ".mp4";
            mProcessImage.writeYUV(outputPath);
        });

    }
}
