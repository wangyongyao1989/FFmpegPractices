package com.wangyao.ffmpegpractice;

import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class FFViewModel extends ViewModel {


    private final MutableLiveData<FRAGMENT_STATUS> switchFragment
            = new MutableLiveData<>();

    public enum FRAGMENT_STATUS {
        MAIN,
        BASIC_TRANING,
        CODEC_TRANING,
        PROCESS_IMAGE,
        PROCESS_AUDIO,
        PROCESS_FILTER,
        PROCESS_HW_CODEC,
        PLAY_MEDIA,

    }


    public MutableLiveData<FRAGMENT_STATUS> getSwitchFragment() {
        return switchFragment;
    }
}
