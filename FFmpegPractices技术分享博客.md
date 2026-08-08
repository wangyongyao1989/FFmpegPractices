# Android FFmpeg 实战：从基础到播放器的完整技术解析

> 项目地址：[https://github.com/wangyongyao1989/FFmpegPractices](https://github.com/wangyongyao1989/FFmpegPractices)

## 前言

FFmpeg 是音视频开发领域绕不开的基石。然而，对于 Android 开发者来说，从零开始搭建一个完整的 FFmpeg 学习工程并非易事——交叉编译、JNI 桥接、C++ 层业务逻辑、UI 层交互，每一层都有不少坑。本文将深入剖析 **FFmpegPractices** 项目，这是一套覆盖 FFmpeg 基础操作、编解码、图像处理、滤镜、硬件编解码和音视频播放的完整 Android 实战工程。文章将从整体架构出发，逐模块解析核心代码逻辑，帮助读者建立从理论到实践的完整认知。

---

## 一、项目整体架构

### 1.1 多模块设计

项目采用 **Gradle 多模块架构**，共 9 个模块，每个模块职责清晰：

| 模块 | 功能 | 编译产物 | 难度 |
|------|------|---------|------|
| `app` | UI 宿主，Fragment 导航 | APK | - |
| `commonLib` | 公共工具类（文件路径、Toast） | AAR | - |
| `basicTraningLib` | FFmpeg 基础（信息读取、封装写入） | `libffmpegpractice.so` | 入门 |
| `codecTraningLib` | FFmpeg 编解码（转码、合并、切割） | `libcodectraning.so` | 进阶 |
| `processImageLib` | 图像提取（YUV/JPG/PNG/BMP/GIF） | `libprocessimage.so` | 进阶 |
| `processAudioLib` | 音频处理（PCM/AAC/WAV） | `libprocessaudio.so` | 进阶 |
| `processFilterLib` | 视频滤镜处理 | `libprocessfilter.so` | 高级 |
| `hwCodecLib` | Android 硬件编解码（MediaCodec） | `libhwcodec.so` | 高级 |
| `playMediaLib` | 音视频播放（OpenSL ES + OpenGL） | `libplaymedialib.so` | 高级 |

### 1.2 三层架构

```
┌─────────────────────────────────────────────┐
│              app 模块（UI 层）                │
│   MainActivity + NavigationRailView 导航      │
│   7 个 Fragment（BaseFragment 模板方法）      │
├─────────────────────────────────────────────┤
│         Library 模块（JNI + 业务层）          │
│   Java: XxxOperate（native 方法声明）         │
│   C++: XxxJniCall.cpp（JNI_OnLoad 动态注册）  │
│   C++: XxxWorker.cpp（FFmpeg 实际操作）       │
├─────────────────────────────────────────────┤
│              底层（预编译 FFmpeg .so）         │
│   avcodec / avformat / avutil / avfilter     │
│   swresample / swscale / libx264             │
└─────────────────────────────────────────────┘
```

UI 层通过 ViewBinding + 按钮触发 native 方法，业务层完成 FFmpeg 操作后通过 JNI 回调将结果传回 UI 层。这种分层设计让每个练习模块可以独立编译和测试。

### 1.3 UI 导航机制

应用采用 **单 Activity + 侧边导航栏 + Fragment 懒加载** 架构：

```java
// MainActivity 通过 ViewModel + LiveData 实现 Fragment 切换
public class FFViewModel extends ViewModel {
    private MutableLiveData<FRAGMENT_STATUS> switchFragment = new MutableLiveData<>();
    public enum FRAGMENT_STATUS {
        MAIN, BASIC, CODEC, IMAGE, AUDIO, FILTER, HW_CODEC, PLAY
    }
}
```

`MainActivity` 观察 LiveData 变化，采用 **hide/show 懒加载** 策略切换 Fragment，避免重复创建：

```java
private void selectionFragment(FRAGMENT_STATUS status) {
    hideTransaction(); // 隐藏所有 Fragment
    switch (status) {
        case BASIC:
            if (basicTraningFragment == null) {
                basicTraningFragment = new BasicTraningFragment();
                fragmentTransaction.add(R.id.fragment_container, basicTraningFragment);
            } else {
                fragmentTransaction.show(basicTraningFragment);
            }
            break;
        // ... 其他 case
    }
}
```

所有练习 Fragment 继承 `BaseFragment`，通过模板方法模式统一初始化流程：

```java
public abstract class BaseFragment extends Fragment {
    @Override
    public View onCreateView(...) {
        View view = getLayoutDataBing(inflater, container);
        initView();
        initData();
        initObserver();
        initListener();
        return view;
    }
    // 5 个抽象方法由子类实现
}
```

---

## 二、JNI 动态注册机制

本项目所有模块统一采用 **JNI 动态注册**（`RegisterNatives`），而非静态注册。这种方式在 `.so` 库加载时自动绑定方法，性能更好且更安全。

### 2.1 Java 端声明

以 `basicTraningLib` 为例，Java 类 `FFmpegOperate` 声明 native 方法：

```java
public class FFmpegOperate {
    static {
        System.loadLibrary("ffmpegpractice"); // 加载 libffmpegpractice.so
    }

    public String getFFmpegVersion() {
        return native_get_ffmpeg_version();
    }

    private native String native_get_ffmpeg_version();
    private native String native_get_video_msg(String fragPath);
    // ... 共 8 个 native 方法
}
```

### 2.2 C++ 端动态注册

C++ 端在 `BasicJniCall.cpp` 中完成方法映射和注册：

```cpp
// 目标 Java 类的全限定名
const char *java_class_name = "com/wangyongyao/basictraninglib/FFmpegOperate";

// 方法签名映射表：{Java方法名, JNI签名, C++函数指针}
static const JNINativeMethod methods[] = {
    {"native_string_from_jni",       "()Ljava/lang/String;",                   (void *) cpp_string_from_jni},
    {"native_get_ffmpeg_version",    "()Ljava/lang/String;",                   (void *) cpp_get_ffmpeg_version},
    {"native_get_video_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},
    {"native_get_media_msg",         "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_msg},
    {"native_get_media_codec_msg",   "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_media_codec_msg},
    {"native_media_copy_to_decodec", "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_media_codec_copy_new_to_codec},
    {"native_write_media_to_mp4",    "(Ljava/lang/String;)I",                  (void *) cpp_write_media_to_mp4},
    {"native_write_media_filter",    "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_write_media_filter},
};

// JNI_OnLoad 在 .so 加载时被调用
JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    if ((vm)->GetEnv((void **) &env, JNI_VERSION_1_6) != JNI_OK) {
        return JNI_ERR;
    }
    jclass clazz = env->FindClass(java_class_name);
    env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0]));
    return JNI_VERSION_1_6;
}
```

### 2.3 C++ 业务对象的生命周期管理

每个 C++ 业务对象使用 **惰性初始化** 作为模块级全局变量，在 `JNI_OnUnload` 中统一置空：

```cpp
FFGetVideoMsg *ffGetVideoMsg; // 全局指针

JNIEXPORT jstring JNICALL
cpp_get_video_msg(JNIEnv *env, jobject thiz, jstring videoPath) {
    const char *cFragPath = env->GetStringUTFChars(videoPath, nullptr);
    if (ffGetVideoMsg == nullptr) {
        ffGetVideoMsg = new FFGetVideoMsg(); // 惰性初始化
    }
    const string &videoString = ffGetVideoMsg->getVideoMsg(cFragPath);
    env->ReleaseStringUTFChars(videoPath, cFragPath);
    return env->NewStringUTF(videoString.c_str());
}

JNIEXPORT void JNICALL JNI_OnUnload(JavaVM *vm, void *reserved) {
    if (ffGetVideoMsg) ffGetVideoMsg = nullptr;
    // ... 其他对象置空
}
```

### 2.4 C++ 回调 Java 的多线程机制

在涉及耗时操作（如视频重编码、合并）的模块中，C++ 需要在子线程中回调 Java。以 `RecodecVideo` 为例：

```cpp
RecodecVideo::RecodecVideo(JNIEnv *env, jobject thiz) {
    env->GetJavaVM(&mJavaVm);            // 保存 JavaVM 指针
    mJavaObj = env->NewGlobalRef(thiz);  // 创建全局引用，防止 GC 回收
}

// 子线程获取 JNIEnv
JNIEnv *RecodecVideo::GetJNIEnv(bool *isAttach) {
    JNIEnv *env;
    *isAttach = false;
    int status = mJavaVm->GetEnv((void **) &env, JNI_VERSION_1_6);
    if (status != JNI_OK) {
        // 当前线程未附加到 JVM，需要手动附加
        status = mJavaVm->AttachCurrentThread(&env, nullptr);
        if (status != JNI_OK) return nullptr;
        *isAttach = true;
    }
    return env;
}

// 回调 Java 的 CppStatusCallback 方法
void RecodecVideo::PostRecodecStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);
    if (pEnv == nullptr) return;
    jmethodID mid = pEnv->GetMethodID(
        pEnv->GetObjectClass(mJavaObj), "CppStatusCallback", "(Ljava/lang/String;)V");
    jstring pJstring = pEnv->NewStringUTF(msg);
    pEnv->CallVoidMethod(mJavaObj, mid, pJstring);
    if (isAttach) {
        mJavaVm->DetachCurrentThread(); // 用完分离
    }
}
```

Java 端对应接收回调：

```java
private void CppStatusCallback(String status) {
    if (mOnStatusMsgListener != null) {
        mOnStatusMsgListener.onStatusMsg(status);
    }
}
```

Fragment 中注册监听器并切换到 UI 线程更新显示：

```java
mCodecOperate.setOnStatusMsgListener(msg -> {
    getActivity().runOnUiThread(() -> {
        mStringBuilder.append(msg);
        mTv.setText(mStringBuilder);
    });
});
```

这种 **Java→C++ 动态注册 + C++→Java 回调** 的双向通信模式贯穿整个项目。

---

## 三、basicTraningLib —— FFmpeg 基础练习

这个模块包含 7 个循序渐进的练习，覆盖了 FFmpeg 最核心的 API 调用模式。

### 3.1 FFmpeg 信息读取三步法

所有信息读取操作都遵循 **打开 → 探测 → 关闭** 的三步法：

```cpp
// FFGetVideoMsg.cpp —— 打开文件并获取基本信息
string FFGetVideoMsg::getVideoMsg(const char *filePath) {
    avformat_alloc_context();                        // 1. 分配上下文
    avformat_open_input(&fmt_ctx, filePath, ...);    // 2. 打开文件
    avformat_find_stream_info(fmt_ctx, nullptr);     // 3. 探测流信息
    
    // 读取信息
    string format = fmt_ctx->iformat->name;          // 封装格式（如 "mp4"）
    int64_t duration = fmt_ctx->duration;             // 总时长（微秒）
    
    avformat_close_input(&fmt_ctx);                   // 4. 关闭
}
```

### 3.2 查找流与解码器信息

`FFGetMediaMsg.cpp` 展示了如何查找视频流和音频流，并获取编解码器信息：

```cpp
// av_find_best_stream 是查找特定类型流的推荐方式
int video_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
int audio_index = av_find_best_stream(fmt_ctx, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);

// 通过 codecpar 获取编解码器 ID
AVStream *video_stream = fmt_ctx->streams[video_index];
AVCodecID codec_id = video_stream->codecpar->codec_id;  // 如 AV_CODEC_ID_H264

// 查找解码器
AVCodec *codec = avcodec_find_decoder(codec_id);
// codec->name → "h264"，codec->type → AVMEDIA_TYPE_VIDEO
```

### 3.3 编解码参数复制

`FFGetMediaCodecCopyNew.cpp` 演示了 `avcodec_parameters_to_context()` 的使用，这是 FFmpeg 4.x+ 的标准做法：

```cpp
AVCodec *video_codec = avcodec_find_decoder(codec_id);
AVCodecContext *video_decode_ctx = avcodec_alloc_context3(video_codec);
// 将流中的编解码参数复制到解码器实例
avcodec_parameters_to_context(video_decode_ctx, video_stream->codecpar);
avcodec_open2(video_decode_ctx, video_codec, nullptr);

// 之后可以从实例中读取详细参数
int width = video_decode_ctx->width;
int height = video_decode_ctx->height;
int gop_size = video_decode_ctx->gop_size;
```

### 3.4 封装写入

`FFWriteMediaToMp4.cpp` 展示了写入 MP4 封装的基本流程——分配输出上下文、创建流、写文件头和文件尾：

```cpp
avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, destPath);
avio_open(&out_fmt_ctx->pb, destPath, AVIO_FLAG_READ_WRITE);

AVCodec *video_codec = avcodec_find_encoder(AV_CODEC_ID_H264);
AVCodecContext *video_encode_ctx = avcodec_alloc_context3(video_codec);
video_encode_ctx->width = 320;
video_encode_ctx->height = 240;

AVStream *dest_video = avformat_new_stream(out_fmt_ctx, video_codec);
// 逆操作：从编码器实例提取参数到流
avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);

avformat_write_header(out_fmt_ctx, nullptr);  // 写文件头
// ... 写入数据包 ...
av_write_trailer(out_fmt_ctx);                 // 写文件尾
```

### 3.5 滤镜图初始化

`FFWriteMediaFilter.cpp` 介绍了 FFmpeg 滤镜系统的基础——`buffer` 作为输入源，`buffersink` 作为输出汇：

```cpp
const AVFilter *buffersrc = avfilter_get_by_name("buffer");
const AVFilter *buffersink = avfilter_get_by_name("buffersink");
AVFilterGraph *filter_graph = avfilter_graph_alloc();

// 构造输入源参数字符串
snprintf(args, sizeof(args),
    "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
    width, height, pix_fmt, time_base.num, time_base.den, ...);

// 创建输入/输出滤镜实例并加入滤镜图
avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);

// 解析滤镜字符串（如 "fps=25"）并配置
avfilter_graph_parse_ptr(filter_graph, "fps=25", &inputs, &outputs, nullptr);
avfilter_graph_config(filter_graph, nullptr);
```

---

## 四、codecTraningLib —— 编解码实战

这个模块是项目的核心，涵盖了完整的解码→编码流水线、流操作（复制、剥离、切割、合并）以及多线程处理。

### 4.1 无转码流复制

`CopyMeidaFile.cpp` 展示了不重新编码的媒体文件复制——直接复制压缩数据包：

```cpp
// 输入端：打开文件、查找流
avformat_open_input(&in_fmt_ctx, srcPath, nullptr, nullptr);
avformat_find_stream_info(in_fmt_ctx, nullptr);
int video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, ...);
int audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, ...);

// 输出端：分配封装、创建流
avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, destPath);
avio_open(&out_fmt_ctx->pb, destPath, AVIO_FLAG_READ_WRITE);

// 原样复制编解码参数
AVStream *dest_video = avformat_new_stream(out_fmt_ctx, nullptr);
avcodec_parameters_copy(dest_video->codecpar, src_video->codecpar);
dest_video->codecpar->codec_tag = 0;

avformat_write_header(out_fmt_ctx, nullptr);

// 循环读取并写入数据包
AVPacket *packet = av_packet_alloc();
while (av_read_frame(in_fmt_ctx, packet) >= 0) {
    if (packet->stream_index == video_index) {
        packet->stream_index = 0;  // 重映射流索引
        av_write_frame(out_fmt_ctx, packet);
    } else if (packet->stream_index == audio_index) {
        packet->stream_index = 1;
        av_write_frame(out_fmt_ctx, packet);
    }
    av_packet_unref(packet);
}
av_write_trailer(out_fmt_ctx);
```

核心思路：`avcodec_parameters_copy()` 直接复制编解码参数，`av_read_frame()` 读取的压缩包通过 `av_write_frame()` 直接写入，全程不涉及解码和编码。

### 4.2 视频切割

`SplitVideoOfMedia.cpp` 实现了按时间范围切割视频，核心在于 `av_seek_frame()` 和时间戳调整：

```cpp
// 将秒转换为时间戳
double begin_time = 5.0, end_time = 15.0;
int64_t begin_pts = begin_time / av_q2d(src_video->time_base);
int64_t end_pts = end_time / av_q2d(src_video->time_base);

// Seek 到最近的关键帧（AVSEEK_FLAG_BACKWARD 向前找）
av_seek_frame(in_fmt_ctx, video_index, begin_pts,
              AVSEEK_FLAG_FRAME | AVSEEK_FLAG_BACKWARD);

int64_t key_frame_pts = -1;
while (av_read_frame(in_fmt_ctx, packet) >= 0) {
    if (packet->stream_index == video_index) {
        if (key_frame_pts == -1) {
            key_frame_pts = packet->pts;  // 记录首帧 PTS
        }
        // 超过结束时间则停止
        if (packet->pts > key_frame_pts + end_pts - begin_pts) {
            break;
        }
        // 调整 PTS/DTS 从 0 开始
        packet->pts -= key_frame_pts;
        packet->dts -= key_frame_pts;
        av_write_frame(out_fmt_ctx, packet);
    }
    av_packet_unref(packet);
}
```

`av_q2d()` 将 `AVRational`（有理数 num/den）转为 `double`，用于秒与时间戳之间的转换。Seek 定位到的并非精确位置，而是最近的关键帧，因为视频解码必须从关键帧开始。

### 4.3 音视频合并与时间戳处理

`MergeAudio.cpp` 将不同来源的视频和音频合并，核心是 `av_compare_ts()` 交错写入和 `av_packet_rescale_ts()` 时间基转换：

```cpp
int64_t last_video_pts = 0, last_audio_pts = 0;
while (1) {
    // 比较不同时间基的两个时间戳
    if (av_compare_ts(last_video_pts, dest_video->time_base,
                      last_audio_pts, dest_audio->time_base) <= 0) {
        // 视频时间戳更小，写视频包
        av_read_frame(video_fmt_ctx, packet);
        // 时间基转换：从源时间基到目标时间基
        av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
        packet->stream_index = 0;
        last_video_pts = packet->pts;
    } else {
        // 音频时间戳更小，写音频包
        av_read_frame(audio_fmt_ctx, packet);
        av_packet_rescale_ts(packet, src_audio->time_base, dest_audio->time_base);
        packet->stream_index = 1;
        last_audio_pts = packet->pts;
    }
    av_write_frame(out_fmt_ctx, packet);
    av_packet_unref(packet);
}
```

### 4.4 视频重编码——完整的解码编码流水线

`RecodecVideo.cpp` 是本项目最核心的文件之一，实现了完整的 **解码→编码** 流水线，并在独立线程中执行。

**整体流程：**

```
JNI 入口
  → new RecodecVideo(env, thiz)        // 保存 JavaVM 和全局引用
  → open_input_file(srcPath)           // 打开输入 + 初始化解码器
  → open_output_file(destPath)         // 创建输出 + 初始化编码器
  → new thread(DoRecoding, this)       // 启动工作线程
  → thread.detach()                    // 后台异步执行
```

**输入文件初始化（解码器）：**

```cpp
int RecodecVideo::open_input_file(const char *src_name) {
    avformat_open_input(&in_fmt_ctx, src_name, nullptr, nullptr);
    avformat_find_stream_info(in_fmt_ctx, nullptr);
    
    video_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_VIDEO, ...);
    src_video = in_fmt_ctx->streams[video_index];
    
    // 查找解码器并初始化
    AVCodec *video_codec = avcodec_find_decoder(src_video->codecpar->codec_id);
    video_decode_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_decode_ctx, src_video->codecpar);
    avcodec_open2(video_decode_ctx, video_codec, nullptr);
    
    // 音频流仅保存引用（不重编码）
    audio_index = av_find_best_stream(in_fmt_ctx, AVMEDIA_TYPE_AUDIO, ...);
    src_audio = in_fmt_ctx->streams[audio_index];
    return 0;
}
```

**输出文件初始化（编码器）：**

```cpp
int RecodecVideo::open_output_file(const char *dest_name) {
    avformat_alloc_output_context2(&out_fmt_ctx, nullptr, nullptr, dest_name);
    avio_open(&out_fmt_ctx->pb, dest_name, AVIO_FLAG_READ_WRITE);
    
    // 使用 libx264 编码器
    AVCodec *video_codec = avcodec_find_encoder_by_name("libx264");
    video_encode_ctx = avcodec_alloc_context3(video_codec);
    avcodec_parameters_to_context(video_encode_ctx, src_video->codecpar);
    
    // 注意：帧率和时间基需要手动设置（avcodec_parameters_to_context 不复制这两个）
    video_encode_ctx->framerate = src_video->r_frame_rate;
    if (video_encode_ctx->framerate.num > 60) {
        video_encode_ctx->framerate = (AVRational){25, 1}; // 防止帧率过高导致灰色帧
    }
    video_encode_ctx->time_base = src_video->time_base;
    video_encode_ctx->gop_size = 12; // GOP 间隔
    
    // 全局头标志（允许系统显示缩略图）
    if (out_fmt_ctx->oformat->flags & AVFMT_GLOBALHEADER) {
        video_encode_ctx->flags = AV_CODEC_FLAG_GLOBAL_HEADER;
    }
    avcodec_open2(video_encode_ctx, video_codec, nullptr);
    
    // 创建输出视频流
    dest_video = avformat_new_stream(out_fmt_ctx, nullptr);
    avcodec_parameters_from_context(dest_video->codecpar, video_encode_ctx);
    
    // 音频流直接复制参数（不重编码）
    AVStream *dest_audio = avformat_new_stream(out_fmt_ctx, nullptr);
    avcodec_parameters_copy(dest_audio->codecpar, src_audio->codecpar);
    
    avformat_write_header(out_fmt_ctx, nullptr);
    return 0;
}
```

**主处理循环：**

```cpp
void RecodecVideo::recodecVideo() {
    AVPacket *packet = av_packet_alloc();
    AVFrame *frame = av_frame_alloc();
    
    while (av_read_frame(in_fmt_ctx, packet) >= 0) {
        if (packet->stream_index == video_index) {
            packet->stream_index = 0;
            recode_video(packet, frame);  // 视频包：解码→编码
        } else {
            packet->stream_index = 1;
            av_write_frame(out_fmt_ctx, packet);  // 音频包：直接写入
        }
        av_packet_unref(packet);
    }
    
    // 传入空包冲走解码缓存
    packet->data = nullptr;
    packet->size = 0;
    recode_video(packet, frame);
    
    // 传入空帧冲走编码缓存
    output_video(nullptr);
    
    av_write_trailer(out_fmt_ctx);
    // ... 释放资源
}
```

**解码过程（recode_video）：**

```cpp
int RecodecVideo::recode_video(AVPacket *packet, AVFrame *frame) {
    // 发送压缩包到解码器
    avcodec_send_packet(video_decode_ctx, packet);
    
    while (1) {
        // 接收解码后的帧
        int ret = avcodec_receive_frame(video_decode_ctx, frame);
        if (ret == AVERROR(EAGAIN)) return 0;  // 需要更多数据
        if (ret == AVERROR_EOF) return 1;      // 解码结束
        
        // H.264 裸流通常没有 PTS，需要手动计算
        if (frame->pts == AV_NOPTS_VALUE) {
            double interval = 1.0 / av_q2d(src_video->r_frame_rate);
            frame->pts = count * interval / av_q2d(src_video->time_base);
            count++;
        }
        output_video(frame);  // 编码并写入
    }
}
```

**编码过程（output_video）：**

```cpp
int RecodecVideo::output_video(AVFrame *frame) {
    // 发送原始帧到编码器
    avcodec_send_frame(video_encode_ctx, frame);
    
    while (1) {
        AVPacket *packet = av_packet_alloc();
        // 接收编码后的压缩包
        int ret = avcodec_receive_packet(video_encode_ctx, packet);
        if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
            return (ret == AVERROR(EAGAIN)) ? 0 : 1;
        }
        
        // 时间基转换
        av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
        packet->stream_index = 0;
        av_write_frame(out_fmt_ctx, packet);
        av_packet_unref(packet);
    }
}
```

这里使用了 FFmpeg 3.1+ 引入的 **新 API**：`avcodec_send_packet()` + `avcodec_receive_frame()`（解码）和 `avcodec_send_frame()` + `avcodec_receive_packet()`（编码），替代了旧版的 `avcodec_decode_video2()` 和 `avcodec_encode_video2()`。新 API 采用推送-拉取模型，支持异步解码和编码。

**Flush 机制** 是一个关键细节：在所有数据包处理完毕后，传入空包（`packet->data = nullptr, packet->size = 0`）可以冲走解码器缓存中剩余的帧；传入空帧（`nullptr`）可以冲走编码器缓存中剩余的包。这是因为解码器和编码器内部有缓冲区，部分帧需要后续帧的到来才能输出。

### 4.5 视频合并

`MergeVideo.cpp` 实现了两个视频的顺序拼接，核心在于第二个视频的 PTS 偏移：

```cpp
void MergeVideo::mergeVideo() {
    // Phase 1: 处理第一个视频
    while (av_read_frame(in_fmt_ctx[0], packet) >= 0) {
        if (packet->stream_index == video_index[0]) {
            recode_video(0, packet, frame, 0);  // begin_pts = 0
        }
        av_packet_unref(packet);
    }
    recode_video(0, empty_packet, frame, 0);  // flush 解码器[0]
    
    // 计算第二个视频的起始 PTS（第一个视频的时长）
    double begin_sec = in_fmt_ctx[0]->duration;  // 微秒
    int64_t begin_pts = begin_sec / (1000000.0 * av_q2d(src_video[0]->time_base));
    
    // Phase 2: 追加第二个视频（每帧 PTS 加上偏移量）
    while (av_read_frame(in_fmt_ctx[1], packet) >= 0) {
        if (packet->stream_index == video_index[1]) {
            recode_video(1, packet, frame, begin_pts);
        }
        av_packet_unref(packet);
    }
    recode_video(1, empty_packet, frame, begin_pts);  // flush 解码器[1]
    
    output_video(nullptr);  // flush 编码器
    av_write_trailer(out_fmt_ctx);
}
```

在 `recode_video` 中，第二个视频的每帧 PTS 需要转换时间基并加上偏移量：

```cpp
if (seq == 1) {
    // 将帧 PTS 从第二个视频的时间基转换到第一个视频的时间基
    int64_t pts = av_rescale_q(frame->pts, src_video[1]->time_base, src_video[0]->time_base);
    frame->pts = pts + begin_video_pts;  // 加上偏移量
}
```

`av_rescale_q()` 的计算公式是 `pts * from_time_base / to_time_base`，确保不同时间基的时间戳正确转换。

---

## 五、processImageLib —— 图像提取与生成

这个模块从视频中提取各种格式的图像，包括 YUV、JPG、PNG、BMP、GIF，以及将图片转为视频。

### 5.1 YUV 提取

`SaveYUVFromVideo.cpp` 直接写入解码后的 YUV420P 原始数据：

```cpp
void save_yuv_file(AVFrame *frame, FILE *fp) {
    // YUV420P: Y 分量全高，U/V 分量半高
    int i = 0;
    // Y 分量
    for (i = 0; i < frame->height; i++) {
        fwrite(frame->data[0] + frame->linesize[0] * i, 1, frame->width, fp);
    }
    // U 分量
    for (i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[1] + frame->linesize[1] * i, 1, frame->width / 2, fp);
    }
    // V 分量
    for (i = 0; i < frame->height / 2; i++) {
        fwrite(frame->data[2] + frame->linesize[2] * i, 1, frame->width / 2, fp);
    }
}
```

`linesize` 可能大于 `width`（因为内存对齐），所以每行只写入 `width` 个字节而非 `linesize` 个。

### 5.2 JPG 提取（Sws 转换）

`SaveJPGSwsFromVideo.cpp` 使用 `sws_scale` 进行像素格式转换后再编码为 JPEG：

```cpp
// 分配图像转换器：YUV420P → YUVJ420P
struct SwsContext *sws_ctx = sws_getContext(
    frame->width, frame->height, AV_PIX_FMT_YUV420P,
    frame->width, frame->height, AV_PIX_FMT_YUVJ420P,
    SWS_BILINEAR, nullptr, nullptr, nullptr);

AVFrame *yuvj_frame = av_frame_alloc();
yuvj_frame->format = AV_PIX_FMT_YUVJ420P;
yuvj_frame->width = frame->width;
yuvj_frame->height = frame->height;
av_image_alloc(yuvj_frame->data, yuvj_frame->linesize,
               frame->width, frame->height, AV_PIX_FMT_YUVJ420P, 1);

// 执行转换
sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
          yuvj_frame->data, yuvj_frame->linesize);

// 使用 MJPEG 编码器编码
AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
avcodec_send_frame(bmp_encode_ctx, yuvj_frame);
avcodec_receive_packet(bmp_encode_ctx, packet);
av_write_frame(out_fmt_ctx, packet);
```

### 5.3 PNG 提取

`SavePNGSwsFromVideo.cpp` 将 YUV 转为 RGB24 后用 PNG 编码器编码：

```cpp
// 关键区别：目标格式为 RGB24，编码器为 PNG
sws_getContext(width, height, AV_PIX_FMT_YUV420P,
               width, height, AV_PIX_FMT_RGB24, SWS_BILINEAR, ...);

AVCodec *codec = avcodec_find_encoder(AV_CODEC_ID_PNG);
```

### 5.4 BMP 提取（手动构造文件头）

`SaveBMPSwsFromVideo.cpp` 的特殊之处在于 BMP 不使用 FFmpeg 编码器，而是手动构造文件头：

```cpp
// 转为 BGR24
sws_getContext(width, height, AV_PIX_FMT_YUV420P,
               width, height, AV_PIX_FMT_BGR24, SWS_BILINEAR, ...);

// 手动构造 BMP 文件头
BITMAPFILEHEADER bmp_header;
bmp_header.bfType = 0x4D42; // "BM"
bmp_header.bfSize = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER) + imageSize;
bmp_header.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);

BITMAPINFOHEADER bmp_info;
bmp_info.biSize = sizeof(BITMAPINFOHEADER);
bmp_info.biWidth = width;
bmp_info.biHeight = height;  // 正数表示从下往上存储
bmp_info.biBitCount = 24;
bmp_info.biCompression = 0;

fwrite(&bmp_header, sizeof(BITMAPFILEHEADER), 1, fp);
fwrite(&bmp_info, sizeof(BITMAPINFOHEADER), 1, fp);

// BMP 格式要求从下往上存储，需要翻转行
for (int i = 0; i < height / 2; i++) {
    memcpy(tmp, &buffer[width * i * 3], width * 3);
    memcpy(&buffer[width * i * 3], &buffer[width * (height - 1 - i) * 3], width * 3);
    memcpy(&buffer[width * (height - 1 - i) * 3], tmp, width * 3);
}
fwrite(buffer, 1, imageSize, fp);
```

### 5.5 图片转视频

`SaveImage2Video.cpp` 将多张图片编码为视频，每张图片重复 100 帧（25fps 下约 4 秒）：

```cpp
// 使用 libx264 编码器
AVCodec *video_codec = avcodec_find_encoder_by_name("libx264");

// 每张图片解码后，通过 sws_scale 转为 YUV420P
sws_scale(sws_ctx, frame->data, frame->linesize, 0, frame->height,
          yuv_frame->data, yuv_frame->linesize);

// 重复 100 帧
int i = 0;
while (i++ < 100) {
    yuv_frame->pts = frame_count;
    avcodec_send_frame(video_encode_ctx, yuv_frame);
    avcodec_receive_packet(video_encode_ctx, packet);
    av_packet_rescale_ts(packet, src_video->time_base, dest_video->time_base);
    av_write_frame(out_fmt_ctx, packet);
    frame_count++;
}
```

---

## 六、processFilterLib —— 视频滤镜处理

这个模块使用 FFmpeg 的 Filter Graph 系统实现各种视频特效。

### 6.1 滤镜图完整实现

`ProcessVideoFilter.cpp` 的 `init_filter()` 方法是滤镜系统的核心：

```cpp
int ProcessVideoFilter::init_filter(const char *filters_desc) {
    // 1. 获取输入/输出滤镜
    const AVFilter *buffersrc = avfilter_get_by_name("buffer");     // 输入源
    const AVFilter *buffersink = avfilter_get_by_name("buffersink"); // 输出汇
    
    // 2. 分配滤镜输入输出参数和滤镜图
    AVFilterInOut *inputs = avfilter_inout_alloc();
    AVFilterInOut *outputs = avfilter_inout_alloc();
    filter_graph = avfilter_graph_alloc();
    
    // 3. 构造输入源参数字符串
    char args[512];
    snprintf(args, sizeof(args),
        "video_size=%dx%d:pix_fmt=%d:time_base=%d/%d:pixel_aspect=%d/%d",
        video_decode_ctx->width, video_decode_ctx->height, video_decode_ctx->pix_fmt,
        src_video->time_base.num, src_video->time_base.den,
        video_decode_ctx->sample_aspect_ratio.num,
        video_decode_ctx->sample_aspect_ratio.den);
    
    // 4. 创建输入滤镜实例
    avfilter_graph_create_filter(&buffersrc_ctx, buffersrc, "in", args, nullptr, filter_graph);
    
    // 5. 创建输出滤镜实例
    avfilter_graph_create_filter(&buffersink_ctx, buffersink, "out", nullptr, nullptr, filter_graph);
    
    // 6. 设置输出像素格式
    enum AVPixelFormat pix_fmts[] = {AV_PIX_FMT_YUV420P, AV_PIX_FMT_NONE};
    av_opt_set_int_list(buffersink_ctx, "pix_fmts", pix_fmts, AV_PIX_FMT_NONE, AV_OPT_SEARCH_CHILDREN);
    
    // 7. 设置输入输出连接
    outputs->name = av_strdup("in");
    outputs->filter_ctx = buffersrc_ctx;
    inputs->name = av_strdup("out");
    inputs->filter_ctx = buffersink_ctx;
    
    // 8. 解析滤镜字符串并配置滤镜图
    avfilter_graph_parse_ptr(filter_graph, filters_desc, &inputs, &outputs, nullptr);
    avfilter_graph_config(filter_graph, nullptr);
    
    avfilter_inout_free(&inputs);
    avfilter_inout_free(&outputs);
    return 0;
}
```

### 6.2 滤镜处理循环

滤镜处理在解码和编码之间插入了一层滤镜处理：

```cpp
void ProcessVideoFilter::recode_video(AVPacket *packet, AVFrame *frame, AVFrame *filt_frame) {
    // 解码
    avcodec_send_packet(video_decode_ctx, packet);
    while (avcodec_receive_frame(video_decode_ctx, frame) >= 0) {
        // 将原始帧送入滤镜输入端
        av_buffersrc_add_frame_flags(buffersrc_ctx, frame, AV_BUFFERSRC_FLAG_KEEP_REF);
        
        // 从滤镜输出端获取处理后的帧
        while (av_buffersink_get_frame(buffersink_ctx, filt_frame) >= 0) {
            output_video(filt_frame);  // 编码并写入
            av_frame_unref(filt_frame);
        }
    }
}
```

### 6.3 从滤镜获取编码器参数

一个精妙的设计是输出编码器的参数直接从 `buffersink` 获取，而非从源视频流复制：

```cpp
video_encode_ctx->framerate = av_buffersink_get_frame_rate(buffersink_ctx);
video_encode_ctx->time_base = av_buffersink_get_time_base(buffersink_ctx);
video_encode_ctx->width = av_buffersink_get_w(buffersink_ctx);
video_encode_ctx->height = av_buffersink_get_h(buffersink_ctx);
video_encode_ctx->pix_fmt = (enum AVPixelFormat) av_buffersink_get_format(buffersink_ctx);
```

这样，如果滤镜改变了帧率或分辨率（如 `fps=15` 或 `scale=iw/3:ih/3`），编码器参数会自动适配。

### 6.4 支持的滤镜字符串

项目支持丰富的 FFmpeg 滤镜命令：

| 滤镜字符串 | 效果 |
|-----------|------|
| `fps=5` | 调节帧率 |
| `setpts=0.5*PTS` | 视频快进（2倍速） |
| `trim=start=2:end=5` | 视频切割 |
| `negate=negate_alpha=false` | 底片特效 |
| `drawbox=x=50:y=20:width=150:height=100:color=white:thickness=fill` | 添加方格 |
| `colorchannelmixer=rr=0.3:rg=0.4:rb=0.3:br=0.3:bg=0.4:bb=0.3` | 彩色转黑白 |
| `eq=brightness=0.1:contrast=1.0:gamma=0.1:saturation=1.0` | 调整明暗对比度 |
| `vignette=angle=PI/4` | 光晕效果 |
| `fade=type=in:start_time=0:duration=2` | 淡入特效 |
| `hflip` / `vflip` | 水平/垂直翻转 |
| `scale=width=iw/3:height=ih/3` | 缩放视频 |
| `rotate=angle=PI/2:out_w=ih:out_h=iw` | 旋转视频 |
| `crop=out_w=iw*2/3:out_h=ih*2/3:x=(in_w-out_w)/2:y=(in_h-out_h)/2` | 裁剪视频 |
| `pad=width=iw+80:height=ih+60:x=40:y=30:color=blue` | 填充视频 |

### 6.5 老电影特效

`ProcessVideoToFilm.cpp` 自动构造老电影胶卷边框效果：

```cpp
// 上下添加黑色边框模拟胶片
"pad=w=iw:h=ih+140:x=0:y=70:color=black"
// 在黑边上绘制白色方块模拟胶片孔
"drawbox=x=20:y=20:w=30:h=30:color=white:t=fill"
```

---

## 七、playMediaLib —— 音视频播放器

这是项目中架构最复杂的模块，实现了 5 种播放方式，其中 `FFMediaPlayer` 是一个完整的音视频同步播放器。

### 7.1 五线程架构

`FFMediaPlayer` 采用 5 线程并发架构：

```
┌──────────────┐
│  demux 线程   │ av_read_frame 分离音视频包到各自队列
└──────┬───────┘
       │
  ┌────┴────┐
  ▼         ▼
┌─────┐  ┌─────┐
│音频   │  │视频   │
│解码   │  │解码   │
│线程   │  │线程   │
└──┬──┘  └──┬──┘
   │        │
   ▼        ▼
┌─────┐  ┌─────┐
│音频   │  │视频   │
│播放   │  │播放   │
│线程   │  │线程   │
└─────┘  └─────┘
```

```cpp
bool FFMediaPlayer::start() {
    // 启动解复用线程
    pthread_create(&mDemuxThread, nullptr, demuxThread, this);
    // 启动视频解码线程
    pthread_create(&mVideoDecodeThread, nullptr, videoDecodeThread, this);
    // 启动视频播放线程
    pthread_create(&mVideoPlayThread, nullptr, videoPlayThread, this);
    
    if (mAudioInfo.codecContext) {
        // 启动音频解码线程
        pthread_create(&mAudioDecodeThread, nullptr, audioDecodeThread, this);
        // 启动音频播放线程
        pthread_create(&mAudioPlayThread, nullptr, audioPlayThread, this);
        // 启动 OpenSL ES 播放
        helper.play();
    }
    mState = STATE_STARTED;
    return true;
}
```

### 7.2 包队列与帧队列

播放器使用 **生产者-消费者模型** 管理数据流：

```cpp
// demux 线程将包分发到各自的队列
void FFMediaPlayer::demux() {
    AVPacket *packet = av_packet_alloc();
    while (!mExit) {
        if (mPause) { usleep(10000); continue; }
        
        // 控制队列大小，避免内存占用过大
        pthread_mutex_lock(&mPacketMutex);
        if (mAudioPackets.size() >= mMaxPackets || mVideoPackets.size() >= mMaxPackets) {
            pthread_cond_wait(&mPacketCond, &mPacketMutex);
            pthread_mutex_unlock(&mPacketMutex);
            continue;
        }
        pthread_mutex_unlock(&mPacketMutex);
        
        if (av_read_frame(mFormatContext, packet) < 0) break;
        
        if (packet->stream_index == mAudioInfo.streamIndex) {
            // 入音频包队列
            pthread_mutex_lock(&mPacketMutex);
            mAudioPackets.push(av_packet_clone(packet));
            pthread_cond_signal(&mPacketCond);
            pthread_mutex_unlock(&mPacketMutex);
        } else if (packet->stream_index == mVideoInfo.streamIndex) {
            // 入视频包队列
            pthread_mutex_lock(&mPacketMutex);
            mVideoPackets.push(av_packet_clone(packet));
            pthread_cond_signal(&mPacketCond);
            pthread_mutex_unlock(&mPacketMutex);
        }
        av_packet_unref(packet);
    }
}
```

### 7.3 音视频同步——以音频为主时钟

音视频同步是播放器最核心的技术难点。本项目采用 **音频时钟作为主时钟** 的策略：

```cpp
double FFMediaPlayer::getMasterClock() {
    return getAudioClock(); // 音频时钟作为主时钟
}

void FFMediaPlayer::syncVideo(double pts) {
    double audioTime = getAudioClock();
    double videoTime = pts;
    double diff = videoTime - audioTime;  // 视频与音频的时间差

    const double syncThreshold = 0.01;   // 10ms 同步阈值
    const double maxFrameDelay = 0.1;    // 最大 100ms 延迟

    if (fabs(diff) < maxFrameDelay) {
        if (diff <= -syncThreshold) {
            // 视频落后于音频，立即显示（不延迟）
            return;
        } else if (diff >= syncThreshold) {
            // 视频超前于音频，延迟显示
            int delay = (int)(diff * 1000000); // 转换为微秒
            usleep(delay);
        }
    }
    // 差异太大（超过 100ms），直接显示（可能是丢帧或跳转）
}
```

同步策略说明：
- 视频落后音频超过 10ms：立即显示当前帧（追赶）
- 视频超前音频超过 10ms：延迟显示（等待）
- 差异在 10ms 以内：认为是同步的，正常显示
- 差异超过 100ms：可能是 seek 跳转或丢帧，直接显示

### 7.4 OpenSL ES 音频播放

`OpenslHelper.cpp` 封装了 OpenSL ES 的完整初始化流程：

```cpp
// 1. 创建引擎
SLresult OpenslHelper::createEngine() {
    slCreateEngine(&engine, 0, NULL, 0, NULL, NULL);
    (*engine)->Realize(engine, SL_BOOLEAN_FALSE);
    (*engine)->GetInterface(engine, SL_IID_ENGINE, &engineItf);
}

// 2. 创建混音器
SLresult OpenslHelper::createMix() {
    (*engineItf)->CreateOutputMix(engineItf, &mix, 0, 0, 0);
    (*mix)->Realize(mix, SL_BOOLEAN_FALSE);
    (*mix)->GetInterface(mix, SL_IID_ENVIRONMENTALREVERB, &envItf);
    (*envItf)->SetEnvironmentalReverbProperties(envItf, &settings);
}

// 3. 创建播放器
SLresult OpenslHelper::createPlayer(int numChannels, long samplesRate, 
                                     int bitsPerSample, int channelMask) {
    // 缓冲区队列（4个缓冲区）
    SLDataLocator_AndroidSimpleBufferQueue buffQueue = {
        SL_DATALOCATOR_ANDROIDSIMPLEBUFFERQUEUE, 4
    };
    
    // PCM 格式
    SLDataFormat_PCM dataFormat_pcm = {
        SL_DATAFORMAT_PCM,
        (SLuint32) numChannels,
        (SLuint32) samplesRate,
        (SLuint32) bitsPerSample,
        (SLuint32) bitsPerSample,
        (SLuint32) channelMask,
        SL_BYTEORDER_LITTLEENDIAN
    };
    
    SLDataSource audioSrc = {&buffQueue, &dataFormat_pcm};
    SLDataLocator_OutputMix dataLocator_outputMix = {SL_DATALOCATOR_OUTPUTMIX, mix};
    SLDataSink audioSink = {&dataLocator_outputMix, NULL};
    
    // 创建播放器
    SLInterfaceID ids[3] = {SL_IID_BUFFERQUEUE, SL_IID_EFFECTSEND, SL_IID_VOLUME};
    SLboolean required[3] = {SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE, SL_BOOLEAN_TRUE};
    (*engineItf)->CreateAudioPlayer(engineItf, &player, &audioSrc, &audioSink, 
                                     3, ids, required);
    (*player)->Realize(player, SL_BOOLEAN_FALSE);
    
    // 获取播放和缓冲区队列接口
    (*player)->GetInterface(player, SL_IID_PLAY, &playItf);
    (*player)->GetInterface(player, SL_IID_BUFFERQUEUE, &bufferQueueItf);
}
```

音频播放线程的流程：FFmpeg 解码 → `swr_convert` 重采样为 S16 → `Enqueue` 送入 OpenSL 队列：

```cpp
// 音频播放线程
void FFMediaPlayer::audioPlay() {
    while (!mExit) {
        AVFrame *aframe = nullptr;
        // 从音频帧队列取帧
        pthread_mutex_lock(&mAudioInfo.audioMutex);
        while (mAudioFrames.empty() && !mExit) {
            pthread_cond_wait(&mAudioInfo.audioCond, &mAudioInfo.audioMutex);
        }
        if (!mAudioFrames.empty()) {
            aframe = mAudioFrames.front();
            mAudioFrames.pop();
        }
        pthread_mutex_unlock(&mAudioInfo.audioMutex);
        
        if (aframe) {
            // 重采样为 S16 立体声
            swr_convert(mAudioInfo.swrContext, &mBuffers[mCurrentBuffer], 
                        aframe->nb_samples, ...);
            
            // 更新音频时钟
            setAudioClock(aframe->pts * av_q2d(
                mFormatContext->streams[mAudioInfo.streamIndex]->time_base));
            
            // 送入 OpenSL 队列
            (*helper.bufferQueueItf)->Enqueue(helper.bufferQueueItf, 
                                               mBuffers[mCurrentBuffer], bufferSize);
            mCurrentBuffer = (mCurrentBuffer + 1) % NUM_BUFFERS;
            av_frame_free(&aframe);
        }
    }
}
```

### 7.5 视频渲染

视频渲染使用 `ANativeWindow` + `sws_scale` 将 YUV 转为 RGBA 后渲染到 Surface：

```cpp
void FFMediaPlayer::renderVideoFrame(AVFrame *frame) {
    ANativeWindow_lock(mNativeWindow, &windowBuffer, nullptr);
    
    // sws_scale 将 YUV420P 转为 RGBA
    sws_scale(mVideoInfo.swsContext, frame->data, frame->linesize, 0, 
              frame->height, dst_data, dst_linesize);
    
    // 拷贝到 ANativeWindow
    uint8_t *dst = (uint8_t *) windowBuffer.bits;
    int dstStride = windowBuffer.stride * 4; // RGBA 每像素 4 字节
    for (int i = 0; i < frame->height; i++) {
        memcpy(dst + i * dstStride, dst_data[0] + i * frame->width * 4, 
               frame->width * 4);
    }
    
    ANativeWindow_unlockAndPost(mNativeWindow);
}
```

### 7.6 OpenGL ES 渲染

`FFGLPlayer.cpp` 提供了更高级的 OpenGL ES 渲染方案，使用自定义着色器。着色器源码存放在 assets 中的 `.glsl` 和 `.vert` 文件中：

```cpp
// EGL 初始化
eglsurfaceViewRender->surfaceCreated(mNativeWindow, nullptr);
eglsurfaceViewRender->setSharderStringPath(vertexPath, fragPath);
eglsurfaceViewRender->surfaceChanged(mWidth, mHeight);

// YUV420P 三个平面合并为连续 buffer 交给 OpenGL 渲染
// Y 平面: width * height
// U 平面: width/2 * height/2
// V 平面: width/2 * height/2
```

`OpenGLShader.cpp` 负责着色器的编译和链接：

```cpp
GLuint OpenGLShader::loadShader(GLenum type, const char *shaderSrc) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &shaderSrc, nullptr);
    glCompileShader(shader);
    // 检查编译状态...
    return shader;
}

GLuint OpenGLShader::createProgram(const char *vertexSrc, const char *fragmentSrc) {
    GLuint vertexShader = loadShader(GL_VERTEX_SHADER, vertexSrc);
    GLuint fragmentShader = loadShader(GL_FRAGMENT_SHADER, fragmentSrc);
    GLuint program = glCreateProgram();
    glAttachShader(program, vertexShader);
    glAttachShader(program, fragmentShader);
    glLinkProgram(program);
    return program;
}
```

---

## 八、hwCodecLib —— Android 硬件编解码

这个模块**不使用 FFmpeg**，完全基于 Android NDK 的 `mediandk` API（AMediaCodec/AMediaExtractor/AMediaMuxer）。

### 8.1 硬件提取器

`HwExtractor.cpp` 使用 `AMediaExtractor` 提取媒体轨道数据：

```cpp
mExtractor = AMediaExtractor_new();
AMediaExtractor_setDataSourceFd(mExtractor, fd, 0, fileSize);
AMediaExtractor_selectTrack(mExtractor, trackId);
mFormat = AMediaExtractor_getTrackFormat(mExtractor, trackId);

// 读取 CSD（Codec Specific Data，如 SPS/PPS）
void *csdBuffer;
size_t csdSize;
AMediaFormat_getBuffer(mFormat, "csd-0", &csdBuffer, &csdSize);

// 读取帧数据
int32_t size = AMediaExtractor_readSampleData(mExtractor, mFrameBuf, kMaxBufferSize);
int64_t pts = AMediaExtractor_getSampleTime(mExtractor);
uint32_t flags = AMediaExtractor_getSampleFlags(mExtractor);
AMediaExtractor_advance(mExtractor);  // 前进到下一帧
```

### 8.2 硬件解码器（同步模式）

`HwDeCodec.cpp` 使用 `AMediaCodec` 进行同步解码：

```cpp
mCodec = AMediaCodec_createDecoderByType(mime);
AMediaCodec_configure(mCodec, mFormat, nullptr, nullptr, 0);
AMediaCodec_start(mCodec);

while (!mSawOutputEOS) {
    // 1. 输入：dequeue 输入缓冲区，填充数据
    ssize_t inIdx = AMediaCodec_dequeueInputBuffer(mCodec, kQueueDequeueTimeoutUs);
    if (inIdx >= 0) {
        size_t bufSize;
        uint8_t *buf = AMediaCodec_getInputBuffer(mCodec, inIdx, &bufSize);
        ssize_t bytesRead = readInputData(buf, bufSize);
        AMediaCodec_queueInputBuffer(mCodec, inIdx, 0, bytesRead, pts, flag);
    }
    
    // 2. 输出：dequeue 输出缓冲区
    AMediaCodecBufferInfo info;
    ssize_t outIdx = AMediaCodec_dequeueOutputBuffer(mCodec, &info, kQueueDequeueTimeoutUs);
    if (outIdx >= 0) {
        size_t bufSize;
        uint8_t *buf = AMediaCodec_getOutputBuffer(mCodec, outIdx, &bufSize);
        fwrite(buf, 1, info.size, mOutFp);  // 写入解码后的数据
        AMediaCodec_releaseOutputBuffer(mCodec, outIdx, false);
        
        if (info.flags & AMEDIACODEC_BUFFER_FLAG_END_OF_STREAM) {
            mSawOutputEOS = true;
        }
    }
}
```

### 8.3 硬件编码器

`HwEnCodec.cpp` 配置编码器参数：

```cpp
AMediaFormat_setString(mFormat, AMEDIAFORMAT_KEY_MIME, "video/avc");
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_WIDTH, width);
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_HEIGHT, height);
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_FRAME_RATE, frameRate);
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_I_FRAME_INTERVAL, iFrameInterval);
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_BIT_RATE, bitrate);
AMediaFormat_setInt32(mFormat, AMEDIAFORMAT_KEY_COLOR_FORMAT, colorFormat);

// PTS 计算
if (!strncmp(mMime, "video/", 6)) {
    presentationTimeUs = mNumInputFrame * (1000000 / mParams.frameRate);
} else {
    presentationTimeUs = (uint64_t)mNumInputFrame * mParams.frameSize * 1000000 / mParams.sampleRate;
}
```

### 8.4 硬件封装器

`HwMuxer.cpp` 使用 `AMediaMuxer` 封装输出文件：

```cpp
mMuxer = AMediaMuxer_new(fd, OUTPUT_FORMAT_MPEG_4);
ssize_t trackIndex = AMediaMuxer_addTrack(mMuxer, mFormat);
AMediaMuxer_start(mMuxer);

// 逐帧写入
AMediaMuxer_writeSampleData(mMuxer, trackIdx, inputBuffer, &frameInfo);

AMediaMuxer_stop(mMuxer);
AMediaMuxer_delete(mMuxer);
```

---

## 九、FFmpeg 核心 API 模式总结

通过以上分析，可以提炼出 FFmpeg 开发中的几个标准 API 调用模式：

### 9.1 文件打开与信息探测

```
avformat_alloc_context()     → 分配上下文
avformat_open_input()        → 打开文件
avformat_find_stream_info()  → 探测流信息
av_find_best_stream()        → 查找视频/音频流索引
avcodec_find_decoder()       → 查找解码器
avcodec_alloc_context3()     → 分配解码器实例
avcodec_parameters_to_context() → 复制流参数到实例
avcodec_open2()              → 打开解码器
```

### 9.2 封装写入

```
avformat_alloc_output_context2()   → 分配输出上下文
avio_open()                         → 打开输出 IO
avcodec_find_encoder()              → 查找编码器
avcodec_alloc_context3()            → 分配编码器实例
avcodec_open2()                     → 打开编码器
avformat_new_stream()               → 创建输出流
avcodec_parameters_from_context()   → 从实例提取参数到流
avformat_write_header()             → 写文件头
av_write_frame() × N                → 写数据包
av_write_trailer()                  → 写文件尾
```

### 9.3 解码编码新 API

```
解码: avcodec_send_packet() → avcodec_receive_frame() (循环)
编码: avcodec_send_frame() → avcodec_receive_packet() (循环)
Flush: 传 nullptr 冲走缓存
```

### 9.4 滤镜处理

```
avfilter_get_by_name("buffer") / ("buffersink")  → 获取输入/输出滤镜
avfilter_graph_alloc()                             → 分配滤镜图
avfilter_graph_create_filter() × 2                 → 创建输入/输出实例
avfilter_graph_parse_ptr()                         → 解析滤镜字符串
avfilter_graph_config()                            → 配置滤镜图
av_buffersrc_add_frame_flags()                     → 送入原始帧
av_buffersink_get_frame()                          → 获取处理后的帧
```

### 9.5 时间戳处理

```
av_q2d(time_base)                    → 有理数转 double
av_rescale_q(pts, from_tb, to_tb)   → 时间基转换
av_packet_rescale_ts(pkt, from, to) → 数据包时间戳转换
av_compare_ts(ts1, tb1, ts2, tb2)    → 跨时间基比较
```

---

## 十、CMake 构建配置

每个 Library 模块通过 `CMakeLists.txt` 编译独立的 `.so`，链接预编译的 FFmpeg 共享库：

```cmake
cmake_minimum_required(VERSION 3.22.1)
project("ffmpegpractice")

set(jnilibs ${CMAKE_SOURCE_DIR}/jniLibs)
set(third-party-libs
    ${jnilibs}/${ANDROID_ABI}/libavcodec.so
    ${jnilibs}/${ANDROID_ABI}/libavformat.so
    ${jnilibs}/${ANDROID_ABI}/libavutil.so
    ${jnilibs}/${ANDROID_ABI}/libavfilter.so
    ${jnilibs}/${ANDROID_ABI}/libswresample.so
    ${jnilibs}/${ANDROID_ABI}/libswscale.so
    ${jnilibs}/${ANDROID_ABI}/libpostproc.so
)

add_library(${CMAKE_PROJECT_NAME} SHARED
    BasicJniCall.cpp
    FFGetVersion.cpp
    FFGetVideoMsg.cpp
    FFGetMediaMsg.cpp
    FFGetMediaCodecMsg.cpp
    FFGetMediaCodecCopyNew.cpp
    FFWriteMediaToMp4.cpp
    FFWriteMediaFilter.cpp
)

target_link_libraries(${CMAKE_PROJECT_NAME}
    android
    log
    ${third-party-libs}
)
```

`codecTraningLib` 额外链接了 `libx264.so`，`playMediaLib` 额外链接了 `OpenSLES`、`EGL`、`GLESv3`，`hwCodecLib` 链接了 `mediandk`（不依赖 FFmpeg）。

项目支持 `arm64-v8a` 和 `armeabi-v7a` 两种架构，FFmpeg 交叉编译脚本位于 `CompilationScript/` 目录下。

---

## 十一、项目亮点与技术价值

### 11.1 渐进式学习路径

项目从最简单的 "获取 FFmpeg 版本" 开始，逐步深入到完整的音视频播放器，覆盖了 FFmpeg 的全部核心功能域：

```
基础信息读取 → 流操作 → 编解码 → 图像处理 → 滤镜 → 硬件编解码 → 播放器
```

每个练习都有独立的 C++ 文件，可以单独阅读和理解。

### 11.2 完整的 JNI 双向通信

项目展示了 JNI 开发的两种核心模式：
- **Java → C++**：动态注册（`RegisterNatives`），性能优于静态注册
- **C++ → Java**：`GetMethodID` + `CallVoidMethod`，支持子线程回调

### 11.3 多线程与线程安全

`RecodecVideo`、`FFMediaPlayer` 等文件展示了 C++ 多线程开发的完整实践：
- `std::thread` + `detach` 异步执行
- `pthread_mutex_t` + `pthread_cond_t` 生产者-消费者模型
- `AttachCurrentThread` / `DetachCurrentThread` 线程附加
- `NewGlobalRef` 防止 Java 对象被 GC 回收

### 11.4 音视频同步实现

`FFMediaPlayer` 的音频主时钟同步策略是实际播放器中的常见方案。通过比较视频帧 PTS 与音频时钟，动态调整视频帧的显示时机，实现音视频同步。

---

## 总结

FFmpegPractices 项目是一份难得的 Android FFmpeg 实战教材。它不仅覆盖了 FFmpeg 的全部核心 API，还展示了从 JNI 桥接到 UI 交互的完整工程实践。通过对本项目的学习，可以掌握：

- FFmpeg 基础 API 的使用模式（打开、探测、解码、编码、封装）
- FFmpeg 滤镜系统的原理和使用方法
- Android NDK 下 JNI 动态注册和多线程回调
- OpenSL ES 音频播放和 ANativeWindow/OpenGL 视频渲染
- 音视频同步的基本策略
- Android MediaCodec 硬件编解码

项目持续更新中，后续计划加入更多音视频处理功能。

> 项目地址：[https://github.com/wangyongyao1989/FFmpegPractices](https://github.com/wangyongyao1989/FFmpegPractices)
