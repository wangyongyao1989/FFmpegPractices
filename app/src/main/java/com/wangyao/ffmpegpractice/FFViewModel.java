package com.wangyao.ffmpegpractice;

import androidx.lifecycle.MutableLiveData;
import androidx.lifecycle.ViewModel;

public class FFViewModel extends ViewModel {


    private final MutableLiveData<FRAGMENT_STATUS> switchFragment
            = new MutableLiveData<>();

    public enum FRAGMENT_STATUS {
        MAIN,
        BASIC_TRANING,

    }


    public MutableLiveData<FRAGMENT_STATUS> getSwitchFragment() {
        return switchFragment;
    }
}
