package com.wangyao.ffmpegpractice;

import android.os.Bundle;
import android.os.FileUtils;
import android.widget.Button;
import android.widget.TextView;

import androidx.appcompat.app.AppCompatActivity;

import com.wangyao.ffmpegpractice.databinding.ActivityMainBinding;
import com.wangyao.ffmpegpractice.ffmpegpractice.FFmpegOperate;
import com.wangyongyao.commonlib.utils.CommonFileUtils;

public class MainActivity extends AppCompatActivity {

    private ActivityMainBinding mBinding;
    private TextView mTv;
    private FFmpegOperate mFFmpegOperate;
    private Button mBtn1;
    private Button mBtn2;
    private String mVideoPath;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        mBinding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(mBinding.getRoot());
        initView();
        initData();
        initEventListener();

    }

    private void initEventListener() {
        mBtn1.setOnClickListener(view -> {
            mTv.setText(mFFmpegOperate.getFFmpegVersion() + "");
        });

        mBtn2.setOnClickListener(view -> {
            mTv.setText(mFFmpegOperate.getVideoMsg(mVideoPath));
        });
    }

    private void initData() {
        mFFmpegOperate = new FFmpegOperate();
        mTv.setText(mFFmpegOperate.stringFromC());
        mVideoPath = CommonFileUtils.getModelFilePath(getApplicationContext()
                , "video.mp4");


    }

    private void initView() {
        // Example of a call to a native method
        mTv = mBinding.sampleText;
        mBtn1 = mBinding.btn1;
        mBtn2 = mBinding.btn2;
    }


}