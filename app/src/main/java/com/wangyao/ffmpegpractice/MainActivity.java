package com.wangyao.ffmpegpractice;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;
import android.widget.FrameLayout;

import androidx.appcompat.app.ActionBar;
import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.ViewModelProvider;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.databinding.ActivityMainBinding;
import com.wangyao.ffmpegpractice.fragment.BasicTraningFragment;
import com.wangyao.ffmpegpractice.fragment.CodecTraningFragment;
import com.wangyao.ffmpegpractice.fragment.MainFragment;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = MainActivity.class.getSimpleName();
    private ActivityMainBinding mBinding;
    private FrameLayout mFlMain;
    private FFViewModel mFFViewModel;
    private MainFragment mMainFragment;
    private BasicTraningFragment mBasicTraningFragment;
    private CodecTraningFragment mCodecTraningFragment;
    private FrameLayout mFlBasicTraning;
    private FrameLayout mFlCodecTraning;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkPermission();
        mBinding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(mBinding.getRoot());
        ActionBar supportActionBar = getSupportActionBar();
        if (supportActionBar != null) {
            supportActionBar.hide();
        }
        if (getRequestedOrientation() != ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE) {
            setRequestedOrientation(ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE);
        }
        getWindow().getDecorView().setSystemUiVisibility(View.SYSTEM_UI_FLAG_FULLSCREEN);//隐藏状态栏
        initView();
        initData();
        initObserver();
        initListener();
        addFragment();

    }

    private void addFragment() {
        mMainFragment = new MainFragment();
        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction mFragmentTransaction = fragmentManager.beginTransaction();
        mFragmentTransaction
                .add(mFlMain.getId(), mMainFragment)
                .commit();
    }

    private void initListener() {

    }

    @SuppressLint("RestrictedApi")
    private void initObserver() {
        mFFViewModel = ViewModelProviders.of(this).get(FFViewModel.class);
        mFFViewModel.getSwitchFragment().observe(this, fragmentStatus -> {
            Log.e(TAG, "initObserver fragmentStatus: "+fragmentStatus);
            selectionFragment(fragmentStatus);
        });
    }

    private void initEventListener() {

    }

    private void initData() {



    }

    private void initView() {
        mFlMain = mBinding.flMain;
        mFlBasicTraning = mBinding.flBasicTraning;
        mFlCodecTraning = mBinding.flCodecTraning;

    }


    private void selectionFragment(FFViewModel.FRAGMENT_STATUS status) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        hideTransaction(fragmentTransaction);
        switch (status) {

            case MAIN: {
                fragmentTransaction.show(mMainFragment);
                fragmentTransaction.commit();
            }
            break;

            case BASIC_TRANING: {
                if (mBasicTraningFragment == null) {
                    mBasicTraningFragment = new BasicTraningFragment();
                    fragmentTransaction
                            .add(mFlBasicTraning.getId(), mBasicTraningFragment);
                }
                fragmentTransaction.show(mBasicTraningFragment);
                fragmentTransaction.commit();
            }
            break;

            case CODEC_TRANING: {
                if (mCodecTraningFragment == null) {
                    mCodecTraningFragment = new CodecTraningFragment();
                    fragmentTransaction
                            .add(mFlCodecTraning.getId(), mCodecTraningFragment);
                }
                fragmentTransaction.show(mCodecTraningFragment);
                fragmentTransaction.commit();
            }
            break;

        }
    }

    private void hideTransaction(FragmentTransaction ftr) {
        if (mMainFragment != null) {
            ftr.hide(mMainFragment);
        }

        if (mBasicTraningFragment != null) {
            ftr.hide(mBasicTraningFragment);
        }

        if (mCodecTraningFragment != null) {
            ftr.hide(mCodecTraningFragment);
        }
    }


    public boolean checkPermission() {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && checkSelfPermission(
                android.Manifest.permission.WRITE_EXTERNAL_STORAGE) != PackageManager.PERMISSION_GRANTED) {
            requestPermissions(new String[]{
                    android.Manifest.permission.READ_EXTERNAL_STORAGE,
                    android.Manifest.permission.CAMERA,
                    android.Manifest.permission.WRITE_EXTERNAL_STORAGE,
                    Manifest.permission.RECORD_AUDIO,
            }, 1);

        }
        return false;
    }
}