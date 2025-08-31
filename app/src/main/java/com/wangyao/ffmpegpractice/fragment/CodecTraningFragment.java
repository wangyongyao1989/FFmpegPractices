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
import com.wangyongyao.commonlib.utils.DirectoryPath;

import java.util.Random;

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
    private Button mBtn4;
    private Button mBtn5;
    private Button mBtn6;
    private Button mBtn7;
    private Button mBtn8;
    private Button mBtn9;
    private Button mBtn10;
    private Button mBtn11;

    private String mVideoPath1;
    private String mVideoPath2;

    private String mAACPath;

    private String mH264Path;

    private Button mBtCodecBack;
    private CodecOperate mCodecOperate;
    private StringBuilder mStringBuilder;

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
        mBtn4 = mBinding.btnCodec4;
        mBtn5 = mBinding.btnCodec5;
        mBtn6 = mBinding.btnCodec6;
        mBtn7 = mBinding.btnCodec7;
        mBtn8 = mBinding.btnCodec8;
        mBtn9 = mBinding.btnCodec9;
        mBtn10 = mBinding.btnCodec10;
        mBtn11 = mBinding.btnCodec11;


    }

    @Override
    public void initData() {
        mCodecOperate = new CodecOperate();
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
        mCodecOperate.setOnStatusMsgListener(msg -> {
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
            });

        });

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

        mBtn4.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            String outputPath = videoDir + "copy.mp4";
            String copyInfo = mCodecOperate.copyMediaFile(mVideoPath1, outputPath);
            mTv.setText(copyInfo);
        });

        mBtn5.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            String outputPath = videoDir + "peelaudio.mp4";
            String info = mCodecOperate.peelAudioOfMedia(mVideoPath2, outputPath);
            mTv.setText(info);
        });

        mBtn6.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            String outputPath = videoDir + "splitvideo.mp4";
            String info = mCodecOperate.splitVideoOfMedia(mVideoPath2, outputPath);
            mTv.setText(info);
        });

        mBtn7.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            String outputPath = videoDir + "merge_audio.mp4";
            String info = mCodecOperate.mergeAudio(mVideoPath1, mAACPath, outputPath);
            mTv.setText(info);
        });

        mBtn8.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1; // 加1是为了包括100
            String outputPath = videoDir + "recodec_video" + randomInt + ".mp4";
            mCodecOperate.recodeVideo(mVideoPath2, outputPath);
        });

        mBtn9.setOnClickListener(view -> {
            String stringFromC = mCodecOperate.stringFromC();
            mTv.setText(stringFromC);
        });

        mBtn10.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1; // 加1是为了包括100
            String outputPath = videoDir + "merge_video" + randomInt + ".mp4";
            mCodecOperate.mergeVideo(mVideoPath1, mVideoPath2, outputPath);
        });

        mBtn11.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1; // 加1是为了包括100
            String outputPath = videoDir + "h264tomp4" + randomInt + ".mp4";
            mCodecOperate.h264ToMP4(mH264Path, outputPath);
        });


    }
}
