# FFmpegPractices

> 一套覆盖 FFmpeg 基础操作、编解码、图像处理、滤镜、硬件编解码和音视频播放的完整 Android 实战工程。

[![License](https://img.shields.io/badge/license-Apache%202.0-blue.svg)](LICENSE)

## 项目简介

FFmpegPractices 是一个 Android 平台的 FFmpeg 学习与实践项目，采用 **Gradle 多模块架构**，从最基础的"获取 FFmpeg 版本"开始，逐步深入到完整的音视频同步播放器，共包含 **29 个练习**，覆盖 FFmpeg 的全部核心功能域。

项目不仅包含 FFmpeg 的 C++ 层调用，还涵盖了 JNI 动态注册、多线程回调、OpenSL ES 音频播放、OpenGL ES 视频渲染、Android MediaCodec 硬件编解码等完整工程实践。

### 技术栈

- FFmpeg（avcodec / avformat / avutil / avfilter / swresample / swscale）
- libx264（H.264 软件编码）
- Android NDK + JNI（C++14）
- OpenSL ES（底层音频播放）
- OpenGL ES 3.0（视频着色器渲染）
- Android MediaCodec / MediaExtractor / MediaMuxer（硬件编解码）
- CMake（原生构建系统）

---

## 项目架构

### 多模块设计

项目共 9 个模块，每个模块职责清晰、可独立编译：

| 模块 | 功能 | 编译产物 | 难度 |
|------|------|---------|------|
| `app` | UI 宿主，Fragment 导航 | APK | - |
| `commonLib` | 公共工具类（文件路径、Toast、文件选择器） | AAR | - |
| `basicTraningLib` | FFmpeg 基础（信息读取、封装写入、滤镜初始化） | `libffmpegpractice.so` | 入门 |
| `codecTraningLib` | FFmpeg 编解码（转码、合并、切割、时间戳处理） | `libcodectraning.so` | 进阶 |
| `processImageLib` | 图像提取（YUV/JPG/PNG/BMP/GIF）与图片转视频 | `libprocessimage.so` | 进阶 |
| `processAudioLib` | 音频处理（PCM/AAC/WAV） | `libprocessaudio.so` | 进阶 |
| `processFilterLib` | 视频滤镜处理（16+ 种滤镜命令） | `libprocessfilter.so` | 高级 |
| `hwCodecLib` | Android 硬件编解码（MediaCodec） | `libhwcodec.so` | 高级 |
| `playMediaLib` | 音视频播放（OpenSL ES + OpenGL + 音视频同步） | `libplaymedialib.so` | 高级 |

### 三层架构

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

UI 层通过 ViewBinding + 按钮触发 native 方法，业务层完成 FFmpeg 操作后通过 JNI 回调将结果传回 UI 层。每个练习模块可以独立编译和测试。

### JNI 通信机制

项目统一采用 **JNI 动态注册**（`RegisterNatives`），在 `.so` 库加载时自动绑定方法：

```cpp
// 方法签名映射表
static const JNINativeMethod methods[] = {
    {"native_get_ffmpeg_version", "()Ljava/lang/String;", (void *) cpp_get_ffmpeg_version},
    {"native_get_video_msg",      "(Ljava/lang/String;)Ljava/lang/String;", (void *) cpp_get_video_msg},
    // ...
};

JNIEXPORT jint JNICALL JNI_OnLoad(JavaVM *vm, void *reserved) {
    JNIEnv *env;
    vm->GetEnv((void **) &env, JNI_VERSION_1_6);
    jclass clazz = env->FindClass("com/wangyongyao/basictraninglib/FFmpegOperate");
    env->RegisterNatives(clazz, methods, sizeof(methods) / sizeof(methods[0]));
    return JNI_VERSION_1_6;
}
```

涉及耗时操作（如视频重编码、播放器）的模块，C++ 在子线程中通过 `AttachCurrentThread` 获取 `JNIEnv`，调用 Java 的 `CppStatusCallback` 方法回调状态：

```cpp
void PostStatusMessage(const char *msg) {
    bool isAttach = false;
    JNIEnv *pEnv = GetJNIEnv(&isAttach);  // 子线程附加
    jmethodID mid = pEnv->GetMethodID(
        pEnv->GetObjectClass(mJavaObj), "CppStatusCallback", "(Ljava/lang/String;)V");
    pEnv->CallVoidMethod(mJavaObj, mid, pEnv->NewStringUTF(msg));
    if (isAttach) mJavaVm->DetachCurrentThread();
}
```

### UI 导航

应用采用 **单 Activity + 侧边导航栏 + Fragment 懒加载** 架构。`MainActivity` 通过 `FFViewModel` 的 `LiveData<FRAGMENT_STATUS>` 驱动 Fragment 切换，采用 hide/show 策略避免重复创建。所有练习 Fragment 继承 `BaseFragment`，通过模板方法模式统一 `initView / initData / initObserver / initListener` 初始化流程。

---

## FFmpeg 交叉编译

项目在 `CompilationScript/` 目录下提供了 FFmpeg 及依赖库的交叉编译脚本，支持 macOS 和 Ubuntu 两种环境：

| 脚本 | 说明 |
|------|------|
| `android_ff_build_mac.sh` | macOS 下交叉编译 FFmpeg for Android |
| `android_ff_build_ubuntu.sh` | Ubuntu 下交叉编译 FFmpeg for Android |
| `android_ff_libx264_mac.sh` | macOS 下 FFmpeg 集成 libx264 交叉编译 |
| `android_ff_libx264_ubuntu.sh` | Ubuntu 下 FFmpeg 集成 libx264 交叉编译 |
| `android_libx264_mac.sh` | macOS 下交叉编译 libx264 for Android |

支持 `arm64-v8a` 和 `armeabi-v7a` 两种 ABI。

交叉编译详细教程可参考系列博客：
- [Android Linux ffmpeg 交叉编译](https://blog.csdn.net/wangyongyao1989/article/details/148927569)
- [macOS 上交叉编译 ffmpeg 及安装 ffmpeg 工具](https://blog.csdn.net/wangyongyao1989/article/details/149468056)
- [macOS 上 ffmpeg 带入 libx264 库交叉编译](https://blog.csdn.net/wangyongyao1989/article/details/151013578)
- [Ubuntu 系统下交叉编译 Android 的 X264 库](https://blog.csdn.net/wangyongyao1989/article/details/150530562)
- [Ubuntu 系统下交叉编译 Android 的 X265 库](https://blog.csdn.net/wangyongyao1989/article/details/149421186)
- [Ubuntu 系统下 FFmpeg 源码编译安装](https://blog.csdn.net/wangyongyao1989/article/details/149536138)

更多博客请访问 CSDN 专栏：[FFmpeg 相关练习](https://blog.csdn.net/wangyongyao1989/category_12996310.html)

---

## 练习目录

### basicTraningLib —— FFmpeg 基础练习

- **练习 0：FFmpeg 交叉编译基础**
  - 获取 FFmpeg 版本信息、编译配置、许可证
  - `FFGetVersion.cpp`

- **练习 1：打开/关闭音视频流并获取基本信息**
  - `avformat_open_input()` 打开音视频文件
  - `avformat_find_stream_info()` 查找流信息
  - 打印输出 format 和 duration 信息
  - `FFGetVideoMsg.cpp`

- **练习 2：打印视频及音频相关信息**
  - `av_find_best_stream()` 找到视频流和音频流索引
  - 打印输出视频流及音频流的相关信息
  - `FFGetMediaMsg.cpp`

- **练习 3：获取视频及音频的解码 ID 及解码器名字**
  - `avcodec_find_decoder()` 查找对应的视频及音频编解码器
  - 打印输出编解码器的相关信息
  - `FFGetMediaCodecMsg.cpp`

- **练习 4：把音视频流中的编码参数复制给解码器实例**
  - `avcodec_parameters_to_context()` 把编解码参数复制给解码器实例
  - 打印输出编解码器实例的相关信息
  - `FFGetMediaCodecCopyNew.cpp`

- **练习 5：写入一个音视频文件的封装实例**
  - `avformat_new_stream()` 创建指定编码器的数据流
  - `avcodec_parameters_from_context()` 把编码器实例中的参数复制给数据流
  - `avformat_write_header()` 写文件头
  - `av_write_trailer()` 写入文件尾
  - `FFWriteMediaToMp4.cpp`

- **练习 6：音视频滤镜的初始化**
  - `avfilter_get_by_name()` 获取输入输出滤镜（buffer / buffersink）
  - `avfilter_inout_alloc()` 分配滤镜的输入输出参数
  - `avfilter_graph_create_filter()` 创建输入输出滤镜实例并添加到滤镜图
  - `avfilter_graph_parse_ptr()` 把过滤字符串描述的图形添加到滤镜图
  - `avfilter_graph_config()` 检查过滤字符串有效性并配置滤镜图
  - `FFWriteMediaFilter.cpp`

---

### codecTraningLib —— FFmpeg 编解码器练习

- **练习 7：获取视频的 fps 及音频的采样率**
  - 打印输出视频的 bit_rate / width / height / fps
  - 打印输出音频的 bit_rate / frame_size / sample_rate / nb_channels
  - `GetMediaMsg.cpp`

- **练习 8：获取媒体文件的时间基准**
  - 基于 `AVStream` 结构体获取音频流及视频流的时间基（time_base）
  - `GetMediaTimeBase.cpp`

- **练习 9：获取媒体文件的时间戳**
  - 基于时间基和 fps 计算出音频及视频的时间戳增量
  - `GetMeidaTimeStamp.cpp`

- **练习 10：原样复制视频文件（无转码 stream copy）**
  - `avcodec_parameters_copy()` 原样复制音视频参数
  - `av_read_frame()` 轮询数据包
  - `av_write_frame()` 分别往文件写入音频及视频数据包
  - `CopyMeidaFile.cpp`

- **练习 11：从视频文件剥离音频流**
  - 只创建视频输出流、只写入视频包，音频包被丢弃
  - `PeelAudioOfMedia.cpp`

- **练习 12：切割视频文件**
  - `av_q2d()` 计算开始及结束切割位置的播放时间戳
  - `av_seek_frame()` 寻找指定时间戳的最近关键帧
  - 调整 PTS/DTS 从 0 开始
  - `SplitVideoOfMedia.cpp`

- **练习 13：合并视频流和音频流（不同来源）**
  - `av_compare_ts()` 比较不同时间基的两个时间戳，交错写入
  - `av_packet_rescale_ts()` 视频和音频时间戳的时间基转换
  - `MergeAudio.cpp`

- **练习 14：对视频流重新编码（多线程 + JNI 回调）**
  - 创建独立线程用于重新编码
  - `open_input_file()` / `open_output_file()` 打开输入输出文件
  - `avcodec_send_packet()` + `avcodec_receive_frame()` 解码
  - `avcodec_send_frame()` + `avcodec_receive_packet()` 编码（libx264）
  - 传入空包冲走解码/编码缓存（flush 机制）
  - `RecodecVideo.cpp`

- **练习 15：合并两个视频文件（顺序拼接）**
  - 创建线程用于视频合并
  - 处理完第一个视频后，计算末尾时间基作为第二个视频开头的 PTS 偏移
  - `av_rescale_q()` 将帧 PTS 从第二个视频的时间基转换到第一个视频的时间基
  - `MergeVideo.cpp`

- **练习 16：把原始的 H264 文件封装成 MP4 格式**
  - 基本流程与练习 15 一致，处理 H.264 裸流（手动计算 PTS）
  - `H264ToMP4.cpp`

---

### processImageLib —— FFmpeg 处理图像

- **练习 17：向视频文件写入 YUV 视频帧数据**
  - `av_frame_alloc()` 分配数据帧，设置像素格式/宽高
  - `av_frame_get_buffer()` 为数据帧分配缓冲区
  - `av_frame_make_writable()` 确保数据帧可写
  - 写入 YUV 数据后编码为视频
  - `WriteYUVFrame.cpp`

- **练习 18：把视频帧保存为 YUV 文件**
  - 解码视频帧后直接写入 YUV420P 三个分量（Y 全高，U/V 半高）
  - `SaveYUVFromVideo.cpp`

- **练习 19：把视频帧保存为 YUV 格式的 JPG 图片**
  - 解码后使用 MJPEG 编码器编码为 JPEG
  - `SaveJPGFromVideo.cpp`

- **练习 20：把视频帧保存为 YUVJ 格式的 JPG 图片**
  - `sws_getContext()` 分配图像转换器（YUV420P → YUVJ420P）
  - `sws_scale()` 转换像素格式后编码
  - `SaveJPGSwsFromVideo.cpp`

- **练习 21：把视频帧保存为 PNG 图片**
  - `sws_getContext()` 目标格式为 `AV_PIX_FMT_RGB24`
  - `avcodec_find_encoder(AV_CODEC_ID_PNG)` 查找 PNG 编码器
  - PNG 编码器依赖 zlib 库，交叉编译需 `--enable-zlib`
  - `SavePNGSwsFromVideo.cpp`

- **练习 22：把视频帧保存为 BMP 图片**
  - `sws_scale()` 将 YUV 转为 BGR24
  - 手动构造 `BITMAPFILEHEADER` 和 `BITMAPINFOHEADER`
  - BMP 格式要求从下往上存储，需翻转行
  - `SaveBMPSwsFromVideo.cpp`

- **练习 23：把视频帧保存为 GIF**
  - `avcodec_find_encoder(AV_CODEC_ID_GIF)` 查找 GIF 编码器
  - 保留多个帧数据
  - `SaveGifSwsOfVideo.cpp`

- **练习 24：把图片转成视频**
  - 打开两个图片文件，解码后通过 `sws_scale()` 转为 YUV420P
  - 使用 libx264 编码器编码为视频，每张图片重复 100 帧
  - `SaveImage2Video.cpp`

---

### processAudioLib —— FFmpeg 处理音频

- **练习 25：解码出音频帧保存为 PCM**
  - `av_sample_fmt_is_planar()` 判断音频采样格式是否为平面模式
  - 平面模式需改为交错模式存储
  - `SavePCMOfMeida.cpp`

- **练习 26：解码出音频帧保存为 AAC**
  - `avcodec_find_encoder(AV_CODEC_ID_AAC)` 查找 AAC 编码器
  - `get_adts_header()` 获取 ADTS 头部
  - 写入 ADTS 头部 + 编码后的 AAC 数据
  - `SaveAACOfMedia.cpp`

- **练习 27：解码出音频帧保存为 WAV**
  - 先解码出 PCM 原始音频数据
  - 在 PCM 数据基础上加入 WAV 头文件信息后拼接 PCM 数据
  - `SaveWavOfMedia.cpp`

---

### processFilterLib —— FFmpeg 滤镜处理

- **练习 28：通过滤镜处理调节视频的帧率及播放速度**
  - `init_filter()` 初始化滤镜图（buffer 输入源 / buffersink 输出汇）
  - `av_buffersrc_add_frame_flags()` 把原始帧添加到输入滤镜缓冲区
  - `av_buffersink_get_frame()` 从输出滤镜获取处理后的过滤帧
  - 输出编码器参数直接从 `buffersink` 获取（自动适配滤镜改变后的帧率/分辨率）
  - `ProcessVideoFilter.cpp`

  支持的滤镜字符串：

  | 滤镜字符串 | 效果 |
  |-----------|------|
  | `fps=5` | 调节帧率 |
  | `setpts=0.5*PTS` | 视频快进（2 倍速） |
  | `trim=start=2:end=5` | 视频切割 |
  | `negate=negate_alpha=false` | 底片特效 |
  | `drawbox=x=50:y=20:width=150:height=100:color=white:thickness=fill` | 添加方格 |
  | `colorchannelmixer=rr=0.3:rg=0.4:rb=0.3:br=0.3:bg=0.4:bb=0.3` | 彩色转黑白 |
  | `colorchannelmixer=rr=0.393:rg=0.769:rb=0.189:gr=0.349:gg=0.686:gb=0.168:br=0.272:bg=0.534:bb=0.131` | 怀旧特效 |
  | `eq=brightness=0.1:contrast=1.0:gamma=0.1:saturation=1.0` | 调整明暗对比度 |
  | `vignette=angle=PI/4` | 光晕效果 |
  | `fade=type=in:start_time=0:duration=2` | 淡入特效 |
  | `hflip` / `vflip` | 水平/垂直翻转 |
  | `scale=width=iw/3:height=ih/3` | 缩放视频 |
  | `rotate=angle=PI/2:out_w=ih:out_h=iw` | 旋转视频 |
  | `crop=out_w=iw*2/3:out_h=ih*2/3:x=(in_w-out_w)/2:y=(in_h-out_h)/2` | 裁剪视频 |
  | `pad=width=iw+80:height=ih+60:x=40:y=30:color=blue` | 填充视频 |

- **练习 29：滤镜老电影怀旧风**
  - 使用 pad 滤镜添加黑色边框模拟胶片
  - 使用 drawbox 滤镜绘制白色方块模拟胶片孔
  - `ProcessVideoToFilm.cpp`

---

### hwCodecLib —— Android 硬件编解码

此模块**不使用 FFmpeg**，完全基于 Android NDK 的 `mediandk` API。

- **硬件提取**：`AMediaExtractor` 提取轨道数据，读取 CSD（SPS/PPS）— `HwExtractor.cpp`
- **硬件封装**：`AMediaMuxer` 封装输出文件 — `HwMuxer.cpp`
- **硬件解码**：`AMediaCodec` 同步/异步解码 — `HwDeCodec.cpp`
- **硬件编码**：`AMediaCodec` 配置编码参数（码率/帧率/I帧间隔/色彩格式） — `HwEnCodec.cpp`
- **转封装**：`MediaTransMuxer.cpp`
- **提取+解码**：`MediaExtratorDecodec.cpp`
- **提取+解码+编码**：`MediaExtratorDecodecEncodec.cpp`

---

### playMediaLib —— 音视频播放

这是项目中架构最复杂的模块，实现了 5 种播放方式：

- **AudioTrack 播放**：C++ 解码 PCM 后回调 Java 创建 AudioTrack 播放 — `PlayAudioTrack.cpp`
- **OpenSL ES 播放**：纯 native 音频播放，FFmpeg 解码 → swr_convert 重采样 → OpenSL 队列 — `FFmpegOpenSLPlayer.cpp` + `OpenslHelper.cpp`
- **SurfaceView 播放**：FFmpeg 解码 → sws_scale 转 RGBA → ANativeWindow 渲染 — `FFSurfacePlayer.cpp`
- **OpenGL ES 播放**：FFmpeg 解码 + OpenGL 着色器渲染（自定义 .glsl / .vert 着色器） — `FFGLPlayer.cpp` + `OpenGLShader.cpp`
- **音视频同步播放**：5 线程并发架构 + 音频主时钟同步 — `FFMediaPlayer.cpp`

**FFMediaPlayer 五线程架构：**

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

**音视频同步策略（音频主时钟）：**

```cpp
void syncVideo(double pts) {
    double diff = pts - getAudioClock();  // 视频与音频的时间差
    const double syncThreshold = 0.01;    // 10ms 同步阈值
    const double maxFrameDelay = 0.1;     // 最大 100ms 延迟
    if (fabs(diff) < maxFrameDelay) {
        if (diff <= -syncThreshold) return;      // 视频落后，立即显示
        else if (diff >= syncThreshold) usleep(diff * 1000000);  // 视频超前，延迟
    }
}
```

---

## FFmpeg 核心 API 模式

### 文件打开与信息探测

```
avformat_alloc_context()        → 分配上下文
avformat_open_input()           → 打开文件
avformat_find_stream_info()     → 探测流信息
av_find_best_stream()           → 查找视频/音频流索引
avcodec_find_decoder()          → 查找解码器
avcodec_alloc_context3()        → 分配解码器实例
avcodec_parameters_to_context() → 复制流参数到实例
avcodec_open2()                 → 打开解码器
```

### 封装写入

```
avformat_alloc_output_context2()  → 分配输出上下文
avio_open()                       → 打开输出 IO
avcodec_find_encoder()            → 查找编码器
avcodec_open2()                   → 打开编码器
avformat_new_stream()             → 创建输出流
avcodec_parameters_from_context() → 从实例提取参数到流
avformat_write_header()           → 写文件头
av_write_frame() × N             → 写数据包
av_write_trailer()                → 写文件尾
```

### 解码编码新 API（FFmpeg 3.1+）

```
解码: avcodec_send_packet() → avcodec_receive_frame() (循环)
编码: avcodec_send_frame() → avcodec_receive_packet() (循环)
Flush: 传 nullptr 冲走缓存
```

### 滤镜处理

```
avfilter_get_by_name("buffer" / "buffersink")  → 获取输入/输出滤镜
avfilter_graph_alloc()                          → 分配滤镜图
avfilter_graph_create_filter() × 2              → 创建输入/输出实例
avfilter_graph_parse_ptr()                      → 解析滤镜字符串
avfilter_graph_config()                         → 配置滤镜图
av_buffersrc_add_frame_flags()                  → 送入原始帧
av_buffersink_get_frame()                       → 获取处理后的帧
```

### 时间戳处理

```
av_q2d(time_base)                     → 有理数转 double
av_rescale_q(pts, from_tb, to_tb)     → 时间基转换
av_packet_rescale_ts(pkt, from, to)   → 数据包时间戳转换
av_compare_ts(ts1, tb1, ts2, tb2)     → 跨时间基比较
```

---

## 构建与运行

### 环境要求

- Android Studio (Arctic Fox 或更高版本)
- Android NDK
- CMake 3.22.1+
- compileSdk 34, minSdk 28, targetSdk 34

### 构建

```bash
# 克隆项目
git clone https://github.com/wangyongyao1989/FFmpegPractices.git

# 构建 Debug APK
./gradlew :app:assembleDebug
```

每个 Library 模块通过 `CMakeLists.txt` 编译独立的 `.so`，链接预编译的 FFmpeg 共享库。预编译的 `.so` 和头文件位于各模块的 `src/main/jniLibs/` 和 `src/main/cpp/includes/` 目录下。

### 测试资源

应用内置了测试媒体文件（位于 `app/src/main/assets/`）：

| 文件 | 类型 |
|------|------|
| `video.mp4` / `midway.mp4` / `woman.mp4` | 测试视频 |
| `fuzhous.aac` / `liudehua.mp3` | 测试音频 |
| `out.h264` | 裸 H264 流 |
| `window.png` / `yao.jpg` | 测试图片 |
| `texture_video_play_frament.glsl` / `texture_video_play_vert.glsl` | OpenGL 着色器 |

---

## 项目亮点

- **渐进式学习路径**：从获取 FFmpeg 版本到完整音视频播放器，29 个练习层层递进
- **完整 JNI 双向通信**：Java→C++ 动态注册 + C++→Java 子线程回调
- **多线程实践**：`std::thread` + `detach`、`pthread_mutex_t` + `pthread_cond_t` 生产者-消费者模型、`AttachCurrentThread` 线程附加
- **音视频同步**：音频主时钟策略，动态调整视频帧显示时机
- **软硬件结合**：同时覆盖 FFmpeg 软件编解码和 Android MediaCodec 硬件编解码
- **多种渲染方案**：ANativeWindow、OpenGL ES 着色器两种视频渲染方式

---

## 相关博客

- CSDN 专栏：[FFmpeg 相关练习](https://blog.csdn.net/wangyongyao1989/category_12996310.html)
- 技术分享博客：[FFmpegPractices技术分享博客.md](FFmpegPractices技术分享博客.md)

---

## License

[Apache License 2.0](LICENSE)
