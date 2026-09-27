# rtspLib —— RTSP 拉流音视频播放器（软/硬解可切换）

> 在 FFmpegPractices 工程内新增的第 10 个模块：用 FFmpeg avformat 拉 RTSP 流，视频支持 **FFmpeg 软解 + ANativeWindow 直绘** 与 **AMediaCodec 硬解直出 Surface** 两条管线并可运行时切换，音频统一走 **FFmpeg 解码 → swr 重采样 → OpenSL ES buffer-queue**，以音频为主时钟做音视频同步，面向「直播边缘」而不是「点播追帧」。

- 编译产物：`librtsp.so`（arm64-v8a / armeabi-v7a）
- 宿主入口：`app` → 主页宫格「RTSP拉流播放」→ `RtspFragment`
- 真机验证设备：HONOR AGM3 平板（Android 10，API 29，arm64）

---

## 一、目录结构

```
rtspLib/
├── build.gradle                      # 与 hwCodecLib 同款配置（externalNativeBuild + jniLibs 指向 cpp/jniLibs）
├── src/main/cpp/
│   ├── CMakeLists.txt                # 链接 android/log/mediandk/OpenSLES + 内置的 9 个 FFmpeg/x264 .so
│   ├── RtspPlayer.{h,cpp}            # 播放器主体：解封装、四条管线线程、同步、重连会话
│   ├── RtspHardDecoder.{h,cpp}       # AMediaCodec（NDK）硬解封装
│   ├── OpenslHelper.{h,cpp}          # OpenSL ES 引擎/混音器/buffer-queue 封装（copy 自 playMediaLib 并按需修正）
│   ├── AndroidThreadManager.cpp      # 线程池（play/stop 任务投递），与工程其它模块共用实现
│   ├── RtspJniCall.cpp               # JNI 动态注册：native_rtsp_play / native_rtsp_stop / native_get_rtsp_version
│   ├── includes/                     # FFmpeg 头文件 + BasicCommon.h/LogUtils.h/ThreadSafeQueue.h
│   └── jniLibs/<abi>/                # 内置 FFmpeg 运行库（见第六节）
└── src/main/java/com/wangyao/rtsplib/RtspOperate.java
```

## 二、对外接口

```java
RtspOperate operate = new RtspOperate();
operate.setOnStatusMsgListener(msg -> { /* 回调线程非 UI，需自行 post */ });
operate.playRtsp("rtsp://192.168.1.4:8554/live", surface, true);  // true = MediaCodec 硬解
operate.stopRtsp();                                                // 任意线程可调
operate.getFFmpegVersion();                                        // 版本宏 + configure 串
```

JNI 侧只有一个 `RtspPlayer` 实例（挂在 Java 对象上），`playRtsp` 走线程池异步起播，`stopRtsp` 只置标志位并唤醒所有队列消费者，真正的 join 与资源回收由看门狗 `finishSession()` 收尾（幂等，只有一个调用者会执行）。

---

## 三、数据流与线程模型

```
                     ┌──────────────┐
  RTSP(TCP) ───────► │ demuxThread  │  av_read_frame，永不阻塞
                     └──┬───────┬───┘
              视频包(≤60)│       │音频包(≤120)   满了丢最老
                   ┌────▼──┐ ┌──▼─────────┐
                   │ 软解   │ │audioDecode │  ≤10 chunks
                   │ videoDecodeThread    │──┐
                   │ 或硬解 videoHardThread│  │
                   └────┬──┘              ┌──▼──────────┐
              帧(≤5)     │                  │audioPlayThread│
                   ┌────▼─────────┐        │ OpenSL Enqueue│
                   │videoRenderThread│      └───┬──────────┘
                   │ sws + lock/blit │          │ bufferQueueCallback
                   └────────────────┘          ▼
                        ▲                mAudioClock（主时钟）
                        └──── waitUntilClock(pts, 40ms) ────┘
```

| 线程 | 职责 | 关键约束 |
|------|------|----------|
| `demuxThread` | `av_read_frame` 分包 | **绝不允许阻塞**：一旦 TCP 读窗口堵住，服务端判定 `reader is too slow` 并断连 |
| `audioDecodeThread` | `avcodec_send/receive` + `swr` → S16 交错 | 与视频解码**不共用锁**（见 §7.2） |
| `audioPlayThread` | OpenSL buffer-queue 灌数，饿死时补 20ms 静音 | 队列满（`NUM_BUFFERS=4`）则等待 |
| `videoDecodeThread`（软） | FFmpeg 解码 → `MAX_VIDEO_FRAMES=5`，满则丢最老 | 丢帧计入 `SOFT-DEC drop` |
| `videoRenderThread`（软） | `sws_scale` → `ANativeWindow_lock/unlockAndPost` | 每帧 `waitUntilClock(pts, 40ms)` |
| `videoHardThread`（硬） | 喂 `AMediaCodec`，输出 buffer 直接 render 到 Surface | 只在关键帧起播/恢复 |
| 看门狗（JNI stop 任务） | join + cleanup + 状态落位 | 幂等 |

---

## 四、实现原理要点

### 4.1 拉流参数

RTSP 强制 `rtsp_transport=tcp`（UDP 在移动网络/NAT 下丢包即花屏），`stimeout` 使 `avformat_open_input` 有界返回；探测后按 `AVMediaType` 取最优视频/音频流。H.264 的 SPS/PPS 从 `extradata` 或码流中提取，交给硬解当 `csd-0/csd-1`。

### 4.2 「直播边缘」策略（本模块与点播播放器最大的差别）

直播流没有「从头播」的语义，任何一次停顿后正确行为都是**跳到最新画面**，而不是把积压的包追完。落地为四条规则：

1. 所有队列有界，满了**丢最老**（包 60/120、帧 5、PCM 块 10），demux 侧绝不 `sleep` 等空位。
2. 会话起点、重连起点一律 `mVideoDropping = true`：丢弃到下一个 `AV_PKT_FLAG_KEY` 才起播，因此起步就是浅队列。
3. 视频正在「丢到关键帧」时，**音频包一起丢**，避免声音先跑到画面前面。
4. 渲染等待主时钟时带上限 `LIVE_MAX_WAIT_US = 40ms`：主时钟被音频管线拖后时，视频不会为了对齐一个陈旧时间戳而整体僵住（该值即 25fps 的一帧，超过就宁可跳过等待）。落后超过 `STALE_FRAME_DROP = 0.5s` 的帧直接丢，计入 `stale`。

### 4.3 软解视频路径

`sws_scale` 用 `SWS_FAST_BILINEAR`，输出 RGBA 后 `ANativeWindow_lock/unlockAndPost`。因为内置库是 `--disable-asm` 构建（标量路径），1920x858 → 全屏宽度的 YUV→RGBA 单帧要 23~45ms，25fps 直接做不到；因此渲染宽度上限压到 `MAX_RENDER_WIDTH = 640`（1920x858 → 640x286），单帧降到 ~13ms 才勉强实时。**结论：软解路径是演示级，实时/低延迟场景应选硬解。**

渲染各阶段单独计时（`RenderStageStat`），日志 `SOFT-REN ... sws= lock= copy= post= wait=`，用于区分「缩放贵」还是「上屏贵」还是「等同步」。

### 4.4 硬解视频路径

`RtspHardDecoder` 用 NDK `AMediaCodec`（`mediandk`）：`AMediaCodec_createDecoderByType("video/avc")` → `configure(codec, window, NULL)` → `start`；输入侧喂 AnnexB（`IDR` 起播），输出侧 `AMediaCodec_releaseOutputBuffer(idx, render=true)` 让 SurfaceFlinger 直接合成，**完全不经过 CPU 拷贝**。`onFormatChanged` 检测到宽高变化会重建配置。硬解失败（`mHardFailed`）自动回退软解管线，用户无感。

### 4.5 音频路径

`swr_alloc_set_opts2` 重采样到 `AV_SAMPLE_FMT_S16` 交错，按声道数生成 `SLDataFormat_PCM`。

> **踩坑记录**：OpenSL ES 的 `SLDataFormat_PCM::samplesPerSecond` 单位是**毫赫兹**，必须传 `sampleRate * 1000`。传 44100 会出现「接口全部返回成功、buffer 回调也在跑、但完全没有声音/时钟不动」。正确写法以 `playMediaLib/FFmpegOpenSLPlayer.cpp` 为准。

### 4.6 音视频同步

主时钟 = 已播放 PCM 的结束时间戳：每次向 buffer-queue 灌数据就把该段结束 pts 压入 `mPtsRing`，OpenSL 的 `bufferQueueCallback`（一个 buffer 播完）弹出最老一项作为 `mAudioClock`。视频按 `SYNC_THRESHOLD = 10ms` 判定「早到就等、晚到就丢」，`MAX_FRAME_DELAY = 100ms` 为最大等待延迟；无音频流（或 OpenSL 不可用）时回退系统时钟 `mSysBase`，保证纯视频流仍可播。

---

## 五、会话、停止与重连

| 场景 | 行为 |
|------|------|
| `requestStop()` | 置 `mStopFlag` + `stop()` 唤醒四个队列消费者；任何线程可调 |
| `finishSession()` | join 全部线程、释放解码器/Surface/OpenSL，状态落位；幂等，看门狗与析构竞争时只有一方执行 |
| 流中断 | `RECONNECT_MAX = 5` 次，**每次尝试间隔 3s**（MediaMTX 侧路径恢复需要几秒；曾出现 0.7s 内 5 次全部打完并放弃、推流端还没起来的情况） |
| 重连成功 | `reconnectSession()`：冲刷四个队列 → 关闭并重开输入/解码器 → `resetAudioPipelineState()` → `mSessionGen++` → 重新丢到关键帧。各线程以 `mSessionGen` 变化感知会话切换，硬解线程据此 flush/重建 |
| 重连耗尽 | `RTSP 流中断（重连失败），会话结束`，UI 收到状态消息，进程与实例保持可用，可再次起播 |

**Java 层 Surface 生命周期约定**：`RtspFragment.startPlay()` 一律先把 `SurfaceView` 置 `GONE` 再 `post(VISIBLE)` 重建 Surface，等 `surfaceCreated` 才真正 `doPlay()`（3s 内不回调则提示重试）。复用旧 Surface 会让第二次起播的 MediaCodec 报 `nativeWindowConnect -22`；软/硬解切换同样依赖这条重建路径。

另外，`MainActivity` 的 `NavigationRail` 最多 7 个菜单项，RTSP 入口只放在主页宫格，不进侧栏（rail 同步逻辑对 `RTSP` 直接 `return`）。

---

## 六、内置 FFmpeg 库：来源与替换过程

工程原本内置 **FFmpeg 6.1.2**，configure 关键串为 `--disable-network --disable-asm --enable-small --enable-libx264`（`libavcodec 60.31.102 / libavutil 58.29.100 / libavfilter 9.12.100`）。`--disable-network` 直接砍掉 `libavformat` 的 RTSP/TCP demux 能力，所以本模块**必须**换库。

流程（脚本 `~/ffmpeg/ff612_build_network.sh <abi>`，源码 `~/ffmpeg/ff612-src` = release 6.1.2，NDK 25.1.8937393 / API 24）：

1. 只把 `--disable-network` 改为 `--enable-network`，其余开关与基线逐位一致；编译期 `libx264` 用 `~/ffmpeg/x264/x264/android/<abi>` 预置件做头文件/库引用（configure 前 `export PKG_CONFIG_PATH=<x264 lib>/pkgconfig`）。
2. 两个 ABI 各编一遍，产出 8 个 `.so` + 8 个头文件目录。
3. **同一个 APK 每个 soname 只能有一份**，因此 7 个模块（`basicTraningLib / codecTraningLib / playMediaLib / processAudioLib / processFilterLib / processImageLib / rtspLib`）的 `jniLibs/<abi>` 与 `includes` 全部一起替换，替换后 `md5` 对齐校验一致。
4. 符号校验：新 `libavcodec.so` 的未定义符号 `x264_encoder_open_165` 与内置 `libx264.so` 的导出符号一致（同为 X264_BUILD 165），**x264 不需要重编，也不需要替换**。中途曾把自编的 `libx264.so` 一并复制进各模块，经符号集比对（两侧导出符号完全相同、新 `libavcodec` 所需 15 个 x264 符号原件全部具备）确认属多余改动后，已回退为工程自带的 `libx264.so`（arm64 `a6cb3f68…` / armv7 `f05b444d…`），并删除 `playMediaLib` 里多出的那份（其 `CMakeLists.txt` 本就未引用 x264）——本次授权范围只有 FFmpeg 八个库。回退后 `libx264` 编码路径复测通过（见 7.4）。

> **教训（已固化）**：中途曾按「授权重编」编过 6.0.1，虽然只改了网络开关，但**跨小版本属于新授权范围**——滤镜模块行为随之改变。替换第三方库必须与原有版本逐位一致、只动必要开关，且换完要跑其它模块真机回归。

---

## 七、真机回测与两个实测修复

### 7.1 回测实验室

| 组件 | 位置 |
|------|------|
| 流媒体服务器 | MediaMTX v1.21.1，`~/rtsp-lab/mediamtx`，`:8554`，日志 `~/rtsp-lab/mediamtx.log` |
| 推流 | `~/ffmpeg/ffmpegbuild/bin/ffmpeg -re -stream_loop -1 -i app/src/main/assets/midway.mp4 -c copy -f rtsp rtsp://127.0.0.1:8554/live`（1920x858@25 H.264 + AAC 44.1k，40.08s 循环，长 GOP ≈10s） |
| 播放端 | HONOR 平板 192.168.1.3，地址 `rtsp://192.168.1.4:8554/live` |

**权威判据是服务端日志**：`reader is too slow, discarding N frames` 一旦出现，说明播放器把 TCP 读窗口堵住了（反压），比任何客户端指标都硬。逐像素截图差异 0 只能说明「这一瞬间画面没变」，直播静止画面同样 0 差异，**不能当冻结判据**；冻结要靠管线自报计数（每 5s 的 `SOFT-DEC / SOFT-REN / HARD-STAT`）。

### 7.2 修复 1：音视频解码锁拆分（软解 18.5fps → 25fps）

首轮 6.1.2 回测软解平均只有 `up=92.6/5s ≈ 18.5fps`，最差窗口 `up=53`，且这些窗口一律是 `audioQ=0 wait=40.3ms`（音频块队列被抽干、主时钟停顿、每帧把 40ms 等待上限顶满）。原因是 `audioDecodeThread` 与软解 `videoDecodeThread` 共用一把 `mCodecMutex`，而软解 1920x858 一次 send/receive 要连续持锁几十毫秒，音频解码被饿死。

改为 `mAudioCodecMutex` / `mVideoCodecMutex` 两把（`play()`、`cleanupAll()`、`reconnectSession()` 里先音频后视频、不嵌套，无死锁风险）后：

| 指标 | 拆分前 | 拆分后 |
|------|--------|--------|
| 上屏帧率 | avg 18.5fps，最差 11fps | **25.0~25.4fps**（`up=125~127/5s`），`wait=0.0ms`，`stale=0` |
| `sws` | 12~15ms | 12.8ms（未变，说明瓶颈从来不是缩放而是饿死） |

### 7.3 修复 2：同一实例二次起播音频哑火

现象：软解跑完停止、立刻切硬解，`HARD-STAT clock=0.00` 恒定、`dumpsys media.audio_flinger` 里该进程 AudioTrack 停在 standby（首会话是 1.32M frames delivered）。根因是 `releaseOpenSL()` 销毁 player 后 OpenSL 不再回调 `processBufferQueue`，`mQueuedBufferCount` 残留为 `NUM_BUFFERS`，新会话的 `audioPlayThread` 一进来就判定「缓冲已满」而永不入队 —— 于是主时钟恒 0，视频退化成每帧顶满 40ms 的定速追赶（看起来仍是 25fps，但同步完全失效）。

修复：把只在 `reconnectSession()` 里做的复位抽成 `resetAudioPipelineState()`（清 `mPtsRing` / `mAudioClock` / `mLastEnqueuedEndPts` / `mQueuedBufferCount` / `mFillIndex` / `mAudioDisabled`），`play()` 与 `reconnectSession()` 都调用。

### 7.4 本轮通过的用例（HONOR 真机 + 6.1.2 网络版）

| 用例 | 结果 |
|------|------|
| 软解起播（会话 1） | 25fps 稳定，`sws≈12.8ms`，`lock/copy/post≈0.8/0.4/0.9ms`，`wait=0` |
| 软解 → 硬解切换（会话 2） | `clock` 6.02→36.06 正常推进，`outFrames=124~126/5s` |
| 硬解 → 软解切换（会话 3） | `up=109~120/5s`，时钟正常 |
| 断流且推流端未恢复 | 5 次重连（间隔 3s）后 `RTSP 流中断（重连失败），会话结束`，实例可再用 |
| 会话 4（失败后重新起播） | `up=123~124/5s`，正常 |
| 会话中途中断、推流端 8s 后重启 | 第 3 次尝试 `RTSP 重连成功，会话已重置`，短暂 `up=42` 追帧后回到 25fps |
| 停止 | 四条线程 `... finished` + `RtspPlayer 已停止` + 两个任务 `completed`，进程存活 |
| 服务端反压 | 全程 `~/rtsp-lab/mediamtx.log` 中 `reader is too slow / discarding` **0 条** |
| 其它模块回归（换库必查） | 滤镜「视频滤镜处理」输出 `2000x918/1002 帧/6,012,337B`，落在作者自己两次运行构成的区间 `6,012,325B ~ 6,012,397B` 之内（换成自编 x264 时为 6,012,289B，回退原件后收敛）；「老电影怀旧风」`5,992,081B`（作者基线 5,992,157/5,992,317B）；`playMediaLib` 的 OpenSL 播放、FF 解码 GL 播放、FF 解码音视频同步均正常收尾 |
| libx264 编码路径（x264 回退原件后复测） | `codecTraningLib`「对视频流重新编码」→ `Success recode file!!!!!!`，`recodec_video12.mp4 = 15,549,645B`；滤镜「视频滤镜处理」`ffprobe` 与作者 8 月产物完全同构：`h264 / 2000x918 / yuv420p / nb_frames=1002` |

> 附注（2026-09-26 已修）：滤镜页「把视频画面转存成PNG」原先会 `SIGABRT`，与换库无关，是 `ProcessVideoToPNG.cpp` 自带缺陷——
> `init_filter(filters_desc)` 传的是成员默认值 `""`（`sFilterCmd` 从未赋给它），且返回值没检查；未连接的 sink 让
> `av_buffersink_get_w/h` 返回 0，编码器以 0x0 打开，随后 `av_buffersink_get_frame()` 在 libavfilter 的
> `avfilter_graph_request_oldest()` 触发断言。证据：6.0.1 与 6.1.2 崩溃栈完全一致；仓库里作者自己 2026-08-08 跑的
> `out_png_filter100.png` 就是 0 字节。修复取三处：① `filters_desc = sFilterCmd.c_str()`（对齐 `ProcessVideoFilter.cpp:55`），
> 空串时回退成 `format=pix_fmts=rgb24`；② 检查 `init_filter()` 返回值，失败就退出不再取 sink 宽高；
> ③ 补 `dest_video->time_base = video_encode_ctx->time_base`（image2 不回填，`av_packet_rescale_ts` 会拿到分母 0）。
> 真机复测：`Success process png file.` 无崩溃，`out_png_filter15.png` = 1,177,981B / 1920x858 RGB，
> 与主机侧 `ffmpeg -vf format=pix_fmts=rgb24 -frames:v 1` 的参考帧**逐像素完全相同**（diff `getbbox()=None`）。

---

## 八、已知限制

1. 软解受 `--disable-asm` 标量 `sws` 限制，渲染宽度压到 640 才勉强 25fps；要全屏软解需重编带 asm 的 FFmpeg（涉及授权，见 §6）。
2. 主时钟由音频管线驱动，音频链路异常（OpenSL 初始化失败、设备被独占）时视频回退系统时钟，此时只做定速播放，不再保证lipsync。
3. 长 GOP（实测 ≈10s）下断流恢复要等下一个 IDR，追帧窗口内帧率会短时偏低，属预期。
4. 重连次数固定 5 次、间隔 3s，未做指数退避；服务器长期不可用会彻底结束会话，需用户再次点击起播。

## 九、快速自查清单

- 「连不上」→ 先看 `~/rtsp-lab/mediamtx.log` 是否有 `is reading from path 'live'`，再确认地址是 `192.168.1.4` 而不是 `127.0.0.1`。
- 「没声音」→ `dumpsys media.audio_flinger | grep <pid>` 看 AudioTrack 是 `A`(active) 还是 `S`(standby)；standby 且 `clock=0.00` 说明音频根本没入队（查 `mQueuedBufferCount` 类状态复位）。
- 「画面卡住」→ 别只看两张截图是否相同，看 `SOFT-REN up=` 是否还在涨，以及服务端有没有 `reader is too slow`。
- 「切换解码方式后花屏/黑屏」→ 确认 Java 层每次都重建了 Surface（`nativeWindowConnect -22` 的根源）。
- 「怀疑库不对」→ 进滤镜页点「获取ffmpeg的参数」，核对 `libavcodec 60.31.102 / libavutil 58.29.100` 与 configure 串里的 `--enable-network`。
