# FFmpegPractice FFmpeg相关的练习

## basicTraningLib —— FFmpeg的基础练习：

- 0.FFmpeg交叉❌编辑基础： 
    - 可参考博客为：
    - [CSDN](https://blog.csdn.net/wangyongyao1989/category_12996310.html)
    - [Android Liunx ffmpeg 交叉编译](https://blog.csdn.net/wangyongyao1989/article/details/148927569)
    - [Ubuntu系统下交叉编译Android的X265库](https://blog.csdn.net/wangyongyao1989/article/details/149421186)
    - [macOs上交叉编译ffmpeg及安装ffmpeg工具](https://blog.csdn.net/wangyongyao1989/article/details/149468056)
    - [Ubuntu系统下FFmpeg源码编译安装](https://blog.csdn.net/wangyongyao1989/article/details/149536138)
    - [macOs上ffmpeg带入libx264库交叉编译](https://blog.csdn.net/wangyongyao1989/article/details/151013578)
    - [Ubuntu系统下交叉编译Android的X264库](https://blog.csdn.net/wangyongyao1989/article/details/150530562)
    - ...等博客里面对FFmpeg的交叉编译进行了详细描述，及编译脚本能直接来用使用和参考
    - *FFGetVersion.cpp*
  
  
- 1.练习一：打开关闭一个音视频流，并获取其内部的信息。
  - 这个例子展示了：传入一个音视频文件；
  - 然后使用ffmpeg中 **avformat_open_input()** 方法的打开音视频文件；
  - 使用 **avformat_find_stream_info()** 查找音视频文件中的流信息；
  - 最后打印输出文件中的 **format** 和 **duration** 的信息。
  - *FFGetVideoMsg.cpp*

- 2.练习二：打开一个音视频文件后，打印出视频及音频相关信息。
  - 通过 **av_find_best_stream()** 找到视频流的索引和音频流的索引；
  - 打印输出视频流及音频流的相关信息。
  - *FFGetMediaMsg.cpp*

- 3.练习三：打开一个音视频文件后，获取视频及音频的解码ID及解码器名字：
  - 通过 **avcodec_find_decoder()** 找到对应的视频及音频编解码器；
  - 打印输出视频及音频的编解码器的相关信息。
  - *FFGetMediaCodecMsg*

- 4.练习四：把音视频流中的编码参数复制给解码器实例参数：
  - 通过 **avcodec_parameters_to_context()** 把视频流及音频流中的编解码参数复制给解码器的实例；
  - 打印输出编解码器实例的相关信息。
  - *FFGetMediaCodecCopyNew.cpp*

- 5.练习五：写入一个音视频文件的封装实例
  - 通过 **avformat_new_stream()** 创建指定编码器的数据流;
  - 通过 **avcodec_parameters_from_context()**  把编码器实例中的参数复制给数据流；
  - 通过 **avformat_write_header()** 写文件头；
  - 通过 **av_write_trailer()** 写入文件尾；
  - *FFWriteMediaToMp4.cpp*

- 6.练习六：音视频滤镜的初始化：
  - **avformat_open_input()** 打开音视频文件;
  - **avformat_find_stream_info()** 查找音视频文件中的流信息;
  - **av_find_best_stream()** 找到视频流的索引;
  - **avcodec_find_decoder()**  查找视频解码器;
  - **avcodec_alloc_context3()** 分配解码器的实例;
  - **avcodec_parameters_to_context** 把视频流中的编解码参数复制给解码器的实例;
  - **avfilter_get_by_name()** 获取输入输出滤镜；
  - **avfilter_inout_alloc** 分配滤镜的输入输出参数；
  - **avfilter_graph_create_filter()** 创建输入输出滤镜的实例，并将其添加到现有的滤镜图;
  - **avfilter_graph_parse_ptr()** 把采用过滤字符串描述的图形添加到滤镜图;
  - **avfilter_graph_config()** 检查过滤字符串的有效性，并配置滤镜图中的所有前后连接和图像格式;
  - *FFWriteMediaFilter.cpp*


## codecTraningLib —— FFmpeg编解码器练习：

- 7.练习七：练习七：获取视频的fps及音频的采样率：
  - 通过 **avcodec_find_decoder()** 查找音视频解码器；
  - 打印输出 视频的比特率（bit_rate）/ 视频画面的宽度（width，height）/视频的帧率（fps）相关信息。
  - 打印输出 音频的比特率（bit_rate）/ 音频帧的大小（frame_size）/ 音频的采样率（sample_rate）/ 音频的声道信息（nb_channels）等；
  - *GetMediaMsg.cpp*

- 8.练习八：获取媒体文件的时间基准:
  - 基于 **AVStream** 的结构体获取音频流及视频流的时间基；
  - *GetMediaTimeBase*

- 9.练习九：获取媒体文件的时间戳：
  - 基于时间基和fps的参数计算出音频及视频的时间戳增量；
  - *GetMeidaTimeStamp*

- 10.练习十：原样复制视频文件：
  - **avformat_open_input()** 打开音视频文件;
  - **avformat_find_stream_info()** 查找音视频文件中的流信息;
  - **av_find_best_stream()** 找到音视频流的索引;
  - **avcodec_find_decoder()**  查找视频解码器;
  - **avformat_alloc_output_context2()** 分配音视频文件的封装实例;
  - **avio_open()** 打开输出流;
  - **avformat_new_stream()** 创建音视频数据流
  - **avcodec_parameters_copy()** 把源文件的音视频参数原样复制
  - **avformat_write_header()** 写文件头;
  - **av_packet_alloc()** 分配数据包;
  - **av_read_frame()** 轮询数据包;
  - **av_write_frame()** 分别往文件写入音频及视频数据包；
  - **av_packet_unref()** 清除数据包;
  - **av_write_trailer** 写文件尾;
  - **av_packet_free()** 释放数据包资源;
  - **avio_close()** 关闭输出流;
  - **avformat_free_context()** 释放封装器的实例;
  - **avformat_close_input()** 关闭音视频文件;
  - *CopyMeidaFile.cpp*

- 11.练习十一：从视频文件剥离音频流:
  - **avformat_open_input()** 打开音视频文件;
  - **avformat_find_stream_info()** 查找音视频文件中的流信息;
  - **av_find_best_stream()** 找到音视频流的索引;
  - **avcodec_find_decoder()**  查找视频解码器;
  - **avformat_alloc_output_context2()** 分配音视频文件的封装实例;
  - **avio_open()** 打开输出流;
  - **avformat_new_stream()** 创建音视频数据流
  - **avcodec_parameters_copy()** 把源文件的音视频参数原样复制
  - **avformat_write_header()** 写文件头;
  - **av_packet_alloc()** 分配数据包;
  - **av_read_frame()** 轮询数据包;
  - **av_write_frame()** *这里只是往文件写入视频的数据包*；
  - **av_packet_unref()** 清除数据包;
  - **av_write_trailer** 写文件尾;
  - **av_packet_free()** 释放数据包资源;
  - **avio_close()** 关闭输出流;
  - **avformat_free_context()** 释放封装器的实例;
  - **avformat_close_input()** 关闭音视频文件;
  - *PeelAudioOfMedia.cpp*

- 12.练习十二：切割视频文件:
  - **avformat_open_input()** 打开音视频文件;
  - **avformat_find_stream_info()** 查找音视频文件中的流信息;
  - **av_find_best_stream()** 找到音视频流的索引;
  - **avcodec_find_decoder()**  查找视频解码器;
  - **avformat_alloc_output_context2()** 分配音视频文件的封装实例;
  - **avio_open()** 打开输出流;
  - **avformat_new_stream()** 创建音视频数据流;
  - **avcodec_parameters_copy()** 把源文件的音视频参数原样复制;
  - **avformat_write_header()** 写文件头;
  - **av_q2d()** *计算出开始及结束切割位置的播放时间戳*；
  - **av_seek_frame()** 寻找指定时间戳的关键帧，并非begin_video_pts所处的精确位置，而是离begin_video_pts最近的关键帧;
  - **av_packet_alloc()** 分配数据包;
  - **av_read_frame()** 轮询数据包;
  - **av_write_frame()** 分别往文件写入音频及视频数据包；
  - **av_packet_unref()** 清除数据包;
  - **av_write_trailer** 写文件尾;
  - **av_packet_free()** 释放数据包资源;
  - **avio_close()** 关闭输出流;
  - **avformat_free_context()** 释放封装器的实例;
  - **avformat_close_input()** 关闭音视频文件;
  - *SplitVideoOfMedia.cpp*

- 13.练习十三：合并视频流和音频流:
  - 基本流程和以上练习中的流程大致一致；
  - **av_packet_rescale_ts()** 区别是视频和音频时间戳的处理；
  - *mergeAudio.cpp*

- 14.练习十四：对视频流重新编码:
  - 创建一个线程用于重新编码操作的使用；
  - **open_input_file()**  打开输入文件;
  - **open_output_file()** 打开输出文件;
  - **av_read_frame()** 轮询数据包；
  - **recode_video()** 对视频帧重新编码;
    - **avcodec_send_packet()** 把未解压的数据包发给解码器实例;
    - **avcodec_receive_frame** 从解码器实例获取还原后的数据帧;
    - **output_video()** 给视频帧编码，并写入压缩后的视频包;
  - 传入一个空包，冲走解码缓存;
  - **av_write_trailer** 写文件尾;
    - 释放数据帧资源/释放数据包资源/关闭输出流/关闭视频解码器的实例
    /释放视频解码器的实例/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例/关闭音视频文件；
  -*RecodecVideo.cpp*

- 15.练习十五：合并两个视频文件：
  - 创建一个线程用于视频合并操作的使用；
  - **open_input_file** 打开第一个输入文件；
  - **open_input_file** 打开第二个输入文件；
  - **open_output_file** 打开输出文件;
  - **av_read_frame()** 轮询第一个视频数据包;
    - **recode_video()** 对视频帧重新编码;
    - **avcodec_send_packet()** 把未解压的数据包发给解码器实例;
    - **avcodec_receive_frame()** 从解码器实例获取还原后的数据帧;
    - **output_video()** 给视频帧编码，并写入压缩后的视频包;
      - **avcodec_send_frame()** 把原始的数据帧发给编码器实例;
      - **avcodec_receive_packet()** 从编码器实例获取压缩后的数据包;
      - **av_packet_rescale_ts()** 把数据包的时间戳从一个时间基转换为另一个时间基;
      - **av_write_frame()** 往文件写入一个数据包;
  - 传入一个空包，冲走第一个视频解码缓存;
  - 然后在末尾追加第二个视频;
  - 计算第一个视频末尾的时间基，作为第二个视频开头的时间基;
  - 重复第一视频轮询/帧重新编码/发数据包给解码器/从解码器获取还原的数据帧/原始数据帧发给编码实例/编码器实例获取压缩后的数据包
    把数据包的时间戳从一个时间基转换为另一个时间基/往文件写入一个数据包——的过程；
  - 传入一个空包，冲走第一个视频解码缓存;
  - **av_write_trailer()** 写文件尾;
  - 释放数据帧资源/释放数据包资源/关闭输出流/关闭视频解码器的实例
    /释放视频解码器的实例/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例/关闭音视频文件；
  - *MergeVideo.cpp*

- 16.练习十六：把原始的H264文件封装成MP4格式的文件:
  - 基本流程和以上练习十五中的流程大致一致；


  

    

    
