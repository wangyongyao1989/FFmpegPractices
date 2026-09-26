package com.wangyao.ffmpegpractice.fragment;

import android.annotation.SuppressLint;
import android.os.Bundle;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.annotation.Nullable;
import androidx.lifecycle.ViewModelProviders;
import androidx.recyclerview.widget.GridLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.wangyao.ffmpegpractice.FFViewModel;
import com.wangyao.ffmpegpractice.R;
import com.wangyao.ffmpegpractice.databinding.FragmentMainLayoutBinding;

import java.util.ArrayList;
import java.util.List;

/**
 * author : wangyongyao https://github.com/wangyongyao1989
 * Create Time : 2025/8/12 17:02
 * Descibe : FFmpegPractice com.wangyao.ffmpegpractice.fragment
 */
public class MainFragment extends BaseFragment {

    private static final String TAG = MainFragment.class.getSimpleName();
    private FFViewModel mFfViewModel;
    private FragmentMainLayoutBinding mBinding;

    @Override
    public View getLayoutDataBing(@NonNull LayoutInflater inflater
            , @Nullable ViewGroup container, @Nullable Bundle savedInstanceState) {
        mBinding = FragmentMainLayoutBinding.inflate(inflater);
        return mBinding.getRoot();
    }

    @Override
    public void initView() {
        mBinding.rvMainMenu.setLayoutManager(new GridLayoutManager(requireContext(), 3));
        List<MenuItem> items = new ArrayList<>();
        items.add(new MenuItem("基础训练", android.R.drawable.ic_menu_manage, FFViewModel.FRAGMENT_STATUS.BASIC_TRANING));
        items.add(new MenuItem("FFmpeg编解码", android.R.drawable.ic_menu_camera, FFViewModel.FRAGMENT_STATUS.CODEC_TRANING));
        items.add(new MenuItem("FFmpeg处理图像", android.R.drawable.ic_menu_gallery, FFViewModel.FRAGMENT_STATUS.PROCESS_IMAGE));
        items.add(new MenuItem("FFmpeg处理音频", android.R.drawable.ic_menu_call, FFViewModel.FRAGMENT_STATUS.PROCESS_AUDIO));
        items.add(new MenuItem("FFmpeg滤镜处理", android.R.drawable.ic_menu_edit, FFViewModel.FRAGMENT_STATUS.PROCESS_FILTER));
        items.add(new MenuItem("Android硬件编解码", android.R.drawable.ic_menu_preferences, FFViewModel.FRAGMENT_STATUS.PROCESS_HW_CODEC));
        items.add(new MenuItem("音视频播放相关", android.R.drawable.ic_menu_slideshow, FFViewModel.FRAGMENT_STATUS.PLAY_MEDIA));
        items.add(new MenuItem("RTSP拉流播放", android.R.drawable.ic_menu_share, FFViewModel.FRAGMENT_STATUS.RTSP));

        MenuAdapter adapter = new MenuAdapter(items, status -> {
            if (mFfViewModel != null) {
                mFfViewModel.getSwitchFragment().postValue(status);
            }
        });
        mBinding.rvMainMenu.setAdapter(adapter);
    }

    @Override
    public void initData() {

    }

    @SuppressLint("RestrictedApi")
    @Override
    public void initObserver() {
        mFfViewModel = ViewModelProviders.of(requireActivity())
                .get(FFViewModel.class);
    }

    @Override
    public void initListener() {

    }

    private static class MenuItem {
        String title;
        int iconRes;
        FFViewModel.FRAGMENT_STATUS status;

        MenuItem(String title, int iconRes, FFViewModel.FRAGMENT_STATUS status) {
            this.title = title;
            this.iconRes = iconRes;
            this.status = status;
        }
    }

    private static class MenuAdapter extends RecyclerView.Adapter<MenuAdapter.ViewHolder> {
        private final List<MenuItem> items;
        private final OnItemClickListener listener;

        MenuAdapter(List<MenuItem> items, OnItemClickListener listener) {
            this.items = items;
            this.listener = listener;
        }

        @NonNull
        @Override
        public ViewHolder onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View view = LayoutInflater.from(parent.getContext()).inflate(R.layout.item_main_menu, parent, false);
            return new ViewHolder(view);
        }

        @Override
        public void onBindViewHolder(@NonNull ViewHolder holder, int position) {
            MenuItem item = items.get(position);
            holder.tvTitle.setText(item.title);
            holder.ivIcon.setImageResource(item.iconRes);
            holder.itemView.setOnClickListener(v -> listener.onItemClick(item.status));
        }

        @Override
        public int getItemCount() {
            return items.size();
        }

        static class ViewHolder extends RecyclerView.ViewHolder {
            TextView tvTitle;
            android.widget.ImageView ivIcon;

            ViewHolder(@NonNull View itemView) {
                super(itemView);
                tvTitle = itemView.findViewById(R.id.tv_title);
                ivIcon = itemView.findViewById(R.id.iv_icon);
            }
        }

        interface OnItemClickListener {
            void onItemClick(FFViewModel.FRAGMENT_STATUS status);
        }
    }
}
