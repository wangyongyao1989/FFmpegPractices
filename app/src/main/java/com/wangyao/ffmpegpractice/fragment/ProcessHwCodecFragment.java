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
import com.wangyongyao.commonlib.utils.DirectoryPath;

import java.util.Random;

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
    private Button mBtnHwCodec2;
    private Button mBtnHwCodec3;
    private Button mBtnHwCodec4;
    private Button mBtnHwCodec5;

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
        mBtnHwCodec2 = mBinding.btnHwCodec2;
        mBtnHwCodec3 = mBinding.btnHwCodec3;
        mBtnHwCodec4 = mBinding.btnHwCodec4;
        mBtnHwCodec5 = mBinding.btnHwCodec5;


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

        mBtnHwCodec2.setOnClickListener(view -> {
            String dataDir = DirectoryPath.createSDCardDataDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = dataDir + "extractor" + randomInt + ".csv";
            CommonFileUtils.createFile(outputPath1);
            mProcessHwCodec.processHwExtractor(mVideoPath1, outputPath1);

        });

        mBtnHwCodec3.setOnClickListener(view -> {
            String dataDir = DirectoryPath.createSDCardDataDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = dataDir + "muxer" + randomInt + ".csv";
            String outputPath2 = dataDir + "muxer" + randomInt + ".mp4";
            CommonFileUtils.createFile(outputPath1);
            String fmt = "mp4";
            mProcessHwCodec.processHwMuxer(mVideoPath1, outputPath1, outputPath2, fmt);
        });

        mBtnHwCodec4.setOnClickListener(view -> {
            String dataDir = DirectoryPath.createSDCardDataDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = dataDir + "decodec" + randomInt + ".csv";
            String outputPath2 = dataDir + "decodec" + randomInt + ".out";
            CommonFileUtils.createFile(outputPath1);
            String codecName = "";
            mProcessHwCodec.processHwDeCodec(mVideoPath1, outputPath1, outputPath2, codecName);
        });

        mBtnHwCodec5.setOnClickListener(view -> {
            String dataDir = DirectoryPath.createSDCardDataDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = dataDir + "extactor2muxer2mp4" + randomInt + ".csv";
            String outputPath2 = dataDir + "muxer2mp4" + randomInt + ".mp4";
            CommonFileUtils.createFile(outputPath1);
            String fmt = "mp4";
            mProcessHwCodec.extactor2Muxer2Mp4(mVideoPath1, outputPath1, outputPath2, fmt);
        });
    }

}
