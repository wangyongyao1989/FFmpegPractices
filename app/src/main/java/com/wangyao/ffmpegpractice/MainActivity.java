package com.wangyao.ffmpegpractice;

import android.Manifest;
import android.annotation.SuppressLint;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.util.Log;
import android.view.View;

import androidx.appcompat.app.AppCompatActivity;
import androidx.fragment.app.FragmentManager;
import androidx.fragment.app.FragmentTransaction;
import androidx.lifecycle.ViewModelProviders;

import com.wangyao.ffmpegpractice.databinding.ActivityMainBinding;
import com.wangyao.ffmpegpractice.fragment.BasicTraningFragment;
import com.wangyao.ffmpegpractice.fragment.CodecTraningFragment;
import com.wangyao.ffmpegpractice.fragment.PlayMeidaFragment;
import com.wangyao.ffmpegpractice.fragment.ProcessHwCodecFragment;
import com.wangyao.ffmpegpractice.fragment.MainFragment;
import com.wangyao.ffmpegpractice.fragment.ProcessAudioFragmnet;
import com.wangyao.ffmpegpractice.fragment.ProcessFilterFragment;
import com.wangyao.ffmpegpractice.fragment.ProcessImageFragment;

public class MainActivity extends AppCompatActivity {

    private static final String TAG = MainActivity.class.getSimpleName();
    private ActivityMainBinding mBinding;
    private FFViewModel mFFViewModel;
    private MainFragment mMainFragment;
    private BasicTraningFragment mBasicTraningFragment;
    private CodecTraningFragment mCodecTraningFragment;
    private ProcessImageFragment mProcessImageFragment;
    private ProcessAudioFragmnet mProcessAudioFragmnet;
    private ProcessFilterFragment mProcessFilterFragment;
    private ProcessHwCodecFragment mProcessHwCodecFragment;
    private PlayMeidaFragment mPlayMeidaFragment;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        checkPermission();
        mBinding = ActivityMainBinding.inflate(getLayoutInflater());
        setContentView(mBinding.getRoot());
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
                .add(mBinding.fragmentContainer.getId(), mMainFragment)
                .commit();
    }

    private void initListener() {
        mBinding.navigationRail.setOnItemSelectedListener(item -> {
            int itemId = item.getItemId();
            if (itemId == R.id.menu_basic) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.BASIC_TRANING);
            } else if (itemId == R.id.menu_codec) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.CODEC_TRANING);
            } else if (itemId == R.id.menu_image) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.PROCESS_IMAGE);
            } else if (itemId == R.id.menu_audio) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.PROCESS_AUDIO);
            } else if (itemId == R.id.menu_filter) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.PROCESS_FILTER);
            } else if (itemId == R.id.menu_hw_codec) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.PROCESS_HW_CODEC);
            } else if (itemId == R.id.menu_play) {
                selectionFragment(FFViewModel.FRAGMENT_STATUS.PLAY_MEDIA);
            }
            return true;
        });
    }

    @SuppressLint("RestrictedApi")
    private void initObserver() {
        mFFViewModel = ViewModelProviders.of(this).get(FFViewModel.class);
        mFFViewModel.getSwitchFragment().observe(this, fragmentStatus -> {
            Log.e(TAG, "initObserver fragmentStatus: " + fragmentStatus);
            updateRailSelection(fragmentStatus);
            selectionFragment(fragmentStatus);
        });
    }

    private void updateRailSelection(FFViewModel.FRAGMENT_STATUS status) {
        if (status == FFViewModel.FRAGMENT_STATUS.MAIN) {
            return;
        }
        int menuId;
        switch (status) {
            case BASIC_TRANING: menuId = R.id.menu_basic; break;
            case CODEC_TRANING: menuId = R.id.menu_codec; break;
            case PROCESS_IMAGE: menuId = R.id.menu_image; break;
            case PROCESS_AUDIO: menuId = R.id.menu_audio; break;
            case PROCESS_FILTER: menuId = R.id.menu_filter; break;
            case PROCESS_HW_CODEC: menuId = R.id.menu_hw_codec; break;
            case PLAY_MEDIA: menuId = R.id.menu_play; break;
            default: return;
        }
        if (mBinding.navigationRail.getSelectedItemId() != menuId) {
            mBinding.navigationRail.setSelectedItemId(menuId);
        }
    }

    private void initEventListener() {

    }

    private void initData() {


    }

    private void initView() {
        View headerView = mBinding.navigationRail.findViewById(R.id.rail_header_logo);
        if (headerView != null) {
            headerView.setOnClickListener(v -> selectionFragment(FFViewModel.FRAGMENT_STATUS.MAIN));
        }
    }


    private void selectionFragment(FFViewModel.FRAGMENT_STATUS status) {
        FragmentManager fragmentManager = getSupportFragmentManager();
        FragmentTransaction fragmentTransaction = fragmentManager.beginTransaction();
        hideTransaction(fragmentTransaction);
        int containerId = mBinding.fragmentContainer.getId();
        switch (status) {

            case MAIN: {
                fragmentTransaction.show(mMainFragment);
            }
            break;

            case BASIC_TRANING: {
                if (mBasicTraningFragment == null) {
                    mBasicTraningFragment = new BasicTraningFragment();
                    fragmentTransaction
                            .add(containerId, mBasicTraningFragment);
                }
                fragmentTransaction.show(mBasicTraningFragment);
            }
            break;

            case CODEC_TRANING: {
                if (mCodecTraningFragment == null) {
                    mCodecTraningFragment = new CodecTraningFragment();
                    fragmentTransaction
                            .add(containerId, mCodecTraningFragment);
                }
                fragmentTransaction.show(mCodecTraningFragment);
            }
            break;

            case PROCESS_IMAGE: {
                if (mProcessImageFragment == null) {
                    mProcessImageFragment = new ProcessImageFragment();
                    fragmentTransaction
                            .add(containerId, mProcessImageFragment);
                }
                fragmentTransaction.show(mProcessImageFragment);
            }
            break;

            case PROCESS_AUDIO: {
                if (mProcessAudioFragmnet == null) {
                    mProcessAudioFragmnet = new ProcessAudioFragmnet();
                    fragmentTransaction
                            .add(containerId, mProcessAudioFragmnet);
                }
                fragmentTransaction.show(mProcessAudioFragmnet);
            }
            break;

            case PROCESS_FILTER: {
                if (mProcessFilterFragment == null) {
                    mProcessFilterFragment = new ProcessFilterFragment();
                    fragmentTransaction
                            .add(containerId, mProcessFilterFragment);
                }
                fragmentTransaction.show(mProcessFilterFragment);
            }
            break;

            case PROCESS_HW_CODEC: {
                if (mProcessHwCodecFragment == null) {
                    mProcessHwCodecFragment = new ProcessHwCodecFragment();
                    fragmentTransaction
                            .add(containerId, mProcessHwCodecFragment);
                }
                fragmentTransaction.show(mProcessHwCodecFragment);
            }
            break;
            case PLAY_MEDIA: {
                if (mPlayMeidaFragment == null) {
                    mPlayMeidaFragment = new PlayMeidaFragment();
                    fragmentTransaction
                            .add(containerId, mPlayMeidaFragment);
                }
                fragmentTransaction.show(mPlayMeidaFragment);
            }
            break;
        }
        fragmentTransaction.commit();
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

        if (mProcessImageFragment != null) {
            ftr.hide(mProcessImageFragment);
        }

        if (mProcessAudioFragmnet != null) {
            ftr.hide(mProcessAudioFragmnet);
        }

        if (mProcessFilterFragment != null) {
            ftr.hide(mProcessFilterFragment);
        }

        if (mProcessHwCodecFragment != null) {
            ftr.hide(mProcessHwCodecFragment);
        }

        if (mPlayMeidaFragment != null) {
            ftr.hide(mPlayMeidaFragment);
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