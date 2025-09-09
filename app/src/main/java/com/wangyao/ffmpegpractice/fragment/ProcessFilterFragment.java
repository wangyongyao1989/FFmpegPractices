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
import com.wangyao.ffmpegpractice.databinding.FragmentProcessFilterLayoutBinding;
import com.wangyao.processfilterlib.ProcessFilterOperate;
import com.wangyongyao.commonlib.utils.CommonFileUtils;
import com.wangyongyao.commonlib.utils.DirectoryPath;

import java.util.Random;

/**
 * @author wangyongyao
 * @package com.wangyao.ffmpegpractice.fragment
 * @date 2025/9/7 11:47
 * @decribe TODO
 * @project
 */
public class ProcessFilterFragment extends BaseFragment {

    private FFViewModel mFfViewModel;
    private FragmentProcessFilterLayoutBinding mBinding;
    private TextView mTv;
    private Button mBtn1;
    private Button mBtn2;
    private Button mBtn3;


    private String mVideoPath1;
    private String mVideoPath2;

    private String mAACPath;

    private String mH264Path;

    private String mWindowPngPath;
    private String mYaoJpgPath;

    private Button mBtCodecBack;
    private ProcessFilterOperate mFilterOperate;
    private StringBuilder mStringBuilder;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentProcessFilterLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mTv = mBinding.tvProcessFilter;
        mBtCodecBack = mBinding.btnProcessFilterBack;
        mBtn1 = mBinding.btnProcessFilter1;
        mBtn2 = mBinding.btnProcessFilter2;
        mBtn3 = mBinding.btnProcessFilter3;


    }

    @Override
    public void initData() {
        mFilterOperate = new ProcessFilterOperate();
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
        mFilterOperate.setOnStatusMsgListener(msg -> {
            getActivity().runOnUiThread(() -> {
                mStringBuilder.append(msg);
                mTv.setText(mStringBuilder);
            });

        });

        mBtCodecBack.setOnClickListener(view -> {
            mFfViewModel.getSwitchFragment().postValue(FFViewModel.FRAGMENT_STATUS.MAIN);
        });

        mBtn1.setOnClickListener(view -> {
            mTv.setText(mFilterOperate.getFFmpegVersion());
        });

        mBtn2.setOnClickListener(view -> {
            String videoDir = DirectoryPath.createVideoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = videoDir + "out_video_filter" + randomInt + ".mp4";
            CommonFileUtils.createFile(outputPath1);
            String filterCmd1 = "fps=5";
            //setpts滤镜实现视频快进
            String filterCmd2 = "setpts=0.5*PTS";
            //trim滤镜实现视频的切割
            String filterCmd3 = "trim=start=2:end=5";
            //negate滤镜实现底片特效
            String filterCmd4 = "negate=negate_alpha=false";
            //drawbox滤镜给视频添加方格
            String filterCmd5 = "drawbox=x=50:y=20:width=150:height=100:color=white:thickness=fill";
            //把彩色画面转成黑白画面
            String filterCmd6 = "format=pix_fmts=rgba,colorchannelmixer=rr=0.3:rg=0.4" +
                    ":rb=0.3:br=0.3:bg=0.4:bb=0.3";
            //把彩色画面转成怀旧特效
            String filterCmd7 = "format=pix_fmts=rgba,colorchannelmixer=rr=0.393:rg=0.769" +
                    ":rb=0.189:gr=0.349:gg=0.686:gb=0.168:br=0.272:bg=0.534:bb=0.131";
            //调整明暗对比度
            String filterCmd8 = "eq=brightness=0.1:contrast=1.0:gamma=0.1:saturation=1.0";
            String filterCmd9 = "eq=brightness=0.1:contrast=1.0:gamma=1.0:saturation=1.0";
            //光晕效果
            String filterCmd10 = "vignette=angle=PI/4";
            //淡入淡出特效
            String filterCmd11 = "fade=type=in:start_time=0:duration=2";
            String filterCmd12 = "fade=type=out:start_frame=TOTAL_FRAMES-25:nb_frames=25";

            //翻转视频方向
            String filterCmd13 = "hflip";
            String filterCmd14 = "vflip";

            //缩放视频
            String filterCmd15 = "scale=width=iw/3:height=ih/3";
            //旋转视频
            String filterCmd16 = "rotate=angle=PI/2:out_w=ih:out_h=iw";
            String filterCmd17 = "rotate=angle=PI/3:out_h=iw*2/3:out_h=iw";
            String filterCmd18 = "rotate=angle=PI/6:out_h=iw*2/3:out_h=iw:fillcolor=white";

            //裁剪视频
            String filterCmd19 = "crop=out_w=iw*2/3:out_h=ih*2/3:x=(in_w-out_w)/2:y=(in_h-out_h)/2";

            //填充视频
            String filterCmd20 = "pad=width=iw+80:height=ih+60:x=40:y=30:color=blue";


            mFilterOperate.processVideoFilter(mVideoPath2, outputPath1, filterCmd20);
        });

        mBtn3.setOnClickListener(view -> {
            String photoDir = DirectoryPath.createPhotoDir(getContext());
            Random rand = new Random();
            int randomInt = rand.nextInt(100) + 1;
            String outputPath1 = photoDir + "out_png_filter" + randomInt + ".png";
            CommonFileUtils.createFile(outputPath1);
            String filterCmd1 = "format=pix_fmts=rgb24";
            mFilterOperate.processVideoToPNG(mVideoPath2, outputPath1, filterCmd1);
        });

    }
}
