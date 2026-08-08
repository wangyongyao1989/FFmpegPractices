package com.wangyongyao.commonlib.utils;

import android.content.Context;
import android.content.Intent;
import android.net.Uri;
import android.text.format.Formatter;
import android.view.LayoutInflater;
import android.view.View;
import android.view.ViewGroup;
import android.widget.ImageView;
import android.widget.TextView;

import androidx.annotation.NonNull;
import androidx.appcompat.app.AlertDialog;
import androidx.core.content.FileProvider;
import androidx.recyclerview.widget.LinearLayoutManager;
import androidx.recyclerview.widget.RecyclerView;

import com.wangyongyao.commonlib.R;

import java.io.File;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Collections;
import java.util.Comparator;
import java.util.Date;
import java.util.List;
import java.util.Locale;

/**
 * 通用文件选择器对话框
 * <p>
 * 用于展示指定目录下的文件列表（视频/音频/图片等），
 * 点击文件项可调用系统应用打开。
 */
public class FilePickerDialog {

    /**
     * 显示文件选择器对话框
     *
     * @param context     上下文
     * @param dirPath     要展示的目录路径
     * @param title       对话框标题
     * @param authority   FileProvider 的 authority,用于授权打开文件
     */
    public static void show(Context context, String dirPath, String title, String authority) {
        if (context == null || dirPath == null) {
            return;
        }
        File dir = new File(dirPath);
        if (!dir.exists() || !dir.isDirectory()) {
            new AlertDialog.Builder(context)
                    .setTitle(title)
                    .setMessage("目录不存在或无文件: " + dirPath)
                    .setPositiveButton("确定", null)
                    .show();
            return;
        }
        File[] files = dir.listFiles();
        final List<File> fileList = new ArrayList<>();
        if (files != null && files.length > 0) {
            for (File f : files) {
                if (f.isFile()) {
                    fileList.add(f);
                }
            }
            // 按最后修改时间倒序
            Collections.sort(fileList, new Comparator<File>() {
                @Override
                public int compare(File o1, File o2) {
                    return Long.compare(o2.lastModified(), o1.lastModified());
                }
            });
        }

        View root = LayoutInflater.from(context)
                .inflate(R.layout.common_dialog_file_picker, null);
        TextView tvDir = root.findViewById(R.id.tv_picker_dir);
        TextView tvEmpty = root.findViewById(R.id.tv_picker_empty);
        RecyclerView rv = root.findViewById(R.id.rv_picker_files);

        tvDir.setText(dirPath);
        rv.setLayoutManager(new LinearLayoutManager(context));
        final FileAdapter adapter = new FileAdapter(context, fileList, authority);
        rv.setAdapter(adapter);

        if (fileList.isEmpty()) {
            tvEmpty.setVisibility(View.VISIBLE);
            rv.setVisibility(View.GONE);
        } else {
            tvEmpty.setVisibility(View.GONE);
            rv.setVisibility(View.VISIBLE);
        }

        new AlertDialog.Builder(context)
                .setTitle(title)
                .setView(root)
                .setNegativeButton("关闭", null)
                .setPositiveButton("刷新", (d, w) -> show(context, dirPath, title, authority))
                .show();
    }

    private static class FileAdapter extends RecyclerView.Adapter<FileAdapter.VH> {
        private final Context context;
        private final List<File> files;
        private final String authority;

        FileAdapter(Context context, List<File> files, String authority) {
            this.context = context;
            this.files = files;
            this.authority = authority;
        }

        @NonNull
        @Override
        public VH onCreateViewHolder(@NonNull ViewGroup parent, int viewType) {
            View v = LayoutInflater.from(context)
                    .inflate(R.layout.common_item_file_picker, parent, false);
            return new VH(v);
        }

        @Override
        public void onBindViewHolder(@NonNull VH h, int position) {
            final File f = files.get(position);
            h.tvName.setText(f.getName());
            h.tvSize.setText(Formatter.formatShortFileSize(context, f.length()));
            String time = new SimpleDateFormat("yyyy-MM-dd HH:mm:ss", Locale.getDefault())
                    .format(new Date(f.lastModified()));
            h.tvTime.setText(time);
            h.ivIcon.setImageResource(getIconRes(f.getName()));

            h.itemView.setOnClickListener(v -> openFile(context, f, authority));
        }

        @Override
        public int getItemCount() {
            return files == null ? 0 : files.size();
        }

        private int getIconRes(String name) {
            String n = name.toLowerCase(Locale.getDefault());
            if (n.endsWith(".mp4") || n.endsWith(".h264") || n.endsWith(".flv")
                    || n.endsWith(".mkv") || n.endsWith(".avi") || n.endsWith(".mov")
                    || n.endsWith(".ts") || n.endsWith(".yuv")) {
                return android.R.drawable.ic_media_play;
            } else if (n.endsWith(".mp3") || n.endsWith(".aac") || n.endsWith(".wav")
                    || n.endsWith(".pcm") || n.endsWith(".flac") || n.endsWith(".ogg")
                    || n.endsWith(".m4a")) {
                return android.R.drawable.ic_media_ff;
            } else if (n.endsWith(".jpg") || n.endsWith(".jpeg") || n.endsWith(".png")
                    || n.endsWith(".bmp") || n.endsWith(".gif") || n.endsWith(".webp")) {
                return android.R.drawable.ic_menu_gallery;
            } else {
                return android.R.drawable.ic_menu_save;
            }
        }

        static class VH extends RecyclerView.ViewHolder {
            ImageView ivIcon;
            TextView tvName;
            TextView tvSize;
            TextView tvTime;

            VH(@NonNull View itemView) {
                super(itemView);
                ivIcon = itemView.findViewById(R.id.iv_file_icon);
                tvName = itemView.findViewById(R.id.tv_file_name);
                tvSize = itemView.findViewById(R.id.tv_file_size);
                tvTime = itemView.findViewById(R.id.tv_file_time);
            }
        }
    }

    private static void openFile(Context context, File file, String authority) {
        try {
            Intent intent = new Intent(Intent.ACTION_VIEW);
            intent.addFlags(Intent.FLAG_GRANT_READ_URI_PERMISSION);
            intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
            Uri uri;
            if (authority != null && !authority.isEmpty()) {
                uri = FileProvider.getUriForFile(context, authority, file);
            } else {
                uri = Uri.fromFile(file);
            }
            String type = guessMimeType(file.getName());
            intent.setDataAndType(uri, type);
            context.startActivity(Intent.createChooser(intent, "打开文件"));
        } catch (Exception e) {
            e.printStackTrace();
            android.widget.Toast.makeText(context,
                    "无法打开文件: " + e.getMessage(), android.widget.Toast.LENGTH_SHORT).show();
        }
    }

    private static String guessMimeType(String name) {
        String n = name.toLowerCase(Locale.getDefault());
        if (n.endsWith(".mp4")) return "video/mp4";
        if (n.endsWith(".h264")) return "video/avc";
        if (n.endsWith(".mkv")) return "video/x-matroska";
        if (n.endsWith(".avi")) return "video/x-msvideo";
        if (n.endsWith(".mov")) return "video/quicktime";
        if (n.endsWith(".ts")) return "video/mp2t";
        if (n.endsWith(".yuv")) return "application/octet-stream";
        if (n.endsWith(".mp3")) return "audio/mpeg";
        if (n.endsWith(".aac")) return "audio/aac";
        if (n.endsWith(".wav")) return "audio/wav";
        if (n.endsWith(".pcm")) return "application/octet-stream";
        if (n.endsWith(".flac")) return "audio/flac";
        if (n.endsWith(".ogg")) return "audio/ogg";
        if (n.endsWith(".m4a")) return "audio/mp4";
        if (n.endsWith(".jpg") || n.endsWith(".jpeg")) return "image/jpeg";
        if (n.endsWith(".png")) return "image/png";
        if (n.endsWith(".bmp")) return "image/bmp";
        if (n.endsWith(".gif")) return "image/gif";
        if (n.endsWith(".webp")) return "image/webp";
        return "*/*";
    }
}
