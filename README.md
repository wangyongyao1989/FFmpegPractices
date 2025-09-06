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
  - *FFGetMediaCodecMsg.cpp*

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
  - *GetMediaTimeBase.cpp*

- 9.练习九：获取媒体文件的时间戳：
  - 基于时间基和fps的参数计算出音频及视频的时间戳增量；
  - *GetMeidaTimeStamp.cpp*

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
  - **avformat_free_context()** 释放封装器的实例
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
  - *H264ToMP4.cpp*


## processImageLib —— FFmpeg处理图像：

- 17.练习十七：向视频文件写入YUV视频帧数据：
  - **open_output_file()** 打开输出文件;
    - **avformat_alloc_output_context2()** 分配音视频文件的封装实例
    - **avio_open()** 打开输出流
    - **avcodec_find_encoder()** 查找编码器
    - **avcodec_alloc_context3** 分配编解码器的实例
    - 设置*video_encode_ctx* 的像素格式/视频画面的宽高/帧率/时间基
    - **avcodec_open2()** 打开编码器的实例
    - **avformat_new_stream()** 创建指定编码器的数据流
    - **avcodec_parameters_from_context** 把编码器实例中的参数复制给数据流
    - **avformat_write_header()** 写文件头
  - **av_frame_alloc()** 分配一个数据帧
  - 设置数据帧的像素格式/视频宽高
  - **av_frame_get_buffer()** 为数据帧分配缓冲区
  - 写入数据帧
    - **av_frame_make_writable()** 确保数据帧是可写的
    - 写入yuv数据
    - **output_video()** 给视频帧编码，并写入压缩后的视频包
  - **av_write_trailer()**  写文件尾
  - 释放数据帧资源/关闭输出流/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例
  - *WriteYUVFrame.cpp*

- 18.练习十八：把视频帧保存为YUV文件
  - 常规的*打开音视频文件*、*查找音视频文件中的流信息*、*找到视频流的索引*、*打开解码器的实例*等操作；
  - **av_read_frame()** 轮询数据包
    - **decode_video()** 对视频帧解码
    - **save_yuv_file()** 把视频帧保存为YUV图像
  - 释放数据帧资源/关闭输出流/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例
  - *SaveYUVFromVideo.cpp*


- 19.练习十九：把视频解码后的视频帧保存为YUV格式的JPG图片
  - 常规的*打开音视频文件*、*查找音视频文件中的流信息*、*找到视频流的索引*、*打开解码器的实例*等操作；
  - **av_read_frame()** 轮询数据包
    - **decode_video()** 对视频帧解码
      - **avcodec_send_packet()** 把未解压的数据包发给解码器实例
      - **avcodec_receive_frame()** 从解码器实例获取还原后的数据帧
      - 轮询数据帧传入*save_jpg_file()*
      - **save_jpg_file()** 把视频帧保存为JPEG图片
        - **avformat_alloc_output_context2()** 分配JPEG文件的封装实例
        - **avcodec_find_encoder()** 查找MJPEG编码器
        - **avcodec_alloc_context3()** 获取编解码器上下文信息
        - 设置*bmp_encode_ctx* 的像素格式/视频宽高/时间基
        - **avcodec_open2()** 打开编码器的实例
        - **avformat_new_stream()** 创建数据流
        - **avformat_write_header()** 写文件头
        - **av_packet_alloc()** 分配一个数据包
        - **avcodec_send_frame()** 把原始的数据帧发给编码器实例
        - **avcodec_receive_packet** 从编码器实例获取压缩后的数据包
          - *av_write_frame* 轮询写数据包
        - av_packet_unref()清除数据包/av_write_trailer()写入文件尾/av_packet_free()释放数据包资源
        - avio_close()关闭输出流/avcodec_close()关闭视频编码器的实例/avcodec_free_context()释放视频编码器的实例
        - avformat_free_context()释放封装器的实例
  - **av_frame_free()** 释放数据帧资源
  - **av_packet_free()** 释放数据包资源
  - **avcodec_close()** 关闭视频解码器的实例
  - **avcodec_free_context()** 释放视频解码器的实例
  - **avformat_close_input()** 关闭音视频文件
  - 释放数据帧资源/关闭输出流/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例
  - *SaveJPGFromVideo.cpp*

- 20.练习二十：把视频解码后的视频帧保存为YUVJ格式的JPG图片
  - 常规的*打开音视频文件*、*查找音视频文件中的流信息*、*找到视频流的索引*、*打开解码器的实例*等操作；
  - **av_read_frame()** 轮询数据包
    - **decode_video()** 对视频帧解码
      - **avcodec_send_packet()** 把未解压的数据包发给解码器实例
      - **avcodec_receive_frame()** 从解码器实例获取还原后的数据帧
      - 轮询数据帧传入*save_jpg_file()*
      - **save_jpg_file()** 把视频帧保存为JPEG图片
        - **sws_getContext()** jpg的像素格式是YUVJ420P,分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
        - **av_frame_alloc()** 分配一个YUVJ数据帧
        - **av_image_alloc()** 分配缓冲区空间，用于存放转换后的图像数据
        - **sws_scale()** 转换器开始处理图像数据，把YUV图像转为YUVJ图像
        - **sws_freeContext()** 释放图像转换器的实例
        - **avformat_alloc_output_context2()** 分配JPEG文件的封装实例
        - **avcodec_find_encoder()** 查找MJPEG编码器
        - **avcodec_alloc_context3()** 获取编解码器上下文信息
        - 设置*bmp_encode_ctx* 的像素格式/视频宽高/时间基
        - **avcodec_open2()** 打开编码器的实例
        - **avformat_new_stream()** 创建数据流
        - **avformat_write_header()** 写文件头
        - **av_packet_alloc()** 分配一个数据包
        - **avcodec_send_frame()** 把YUVJ数据帧发给编码器实例
        - **avcodec_receive_packet** 从编码器实例获取压缩后的数据包
          - *av_write_frame* 轮询写数据包
        - av_packet_unref()清除数据包/av_write_trailer()写入文件尾/av_packet_free()释放数据包资源
        - avio_close()关闭输出流/avcodec_close()关闭视频编码器的实例/avcodec_free_context()释放视频编码器的实例
        - avformat_free_context()释放封装器的实例
  - **av_frame_free()** 释放数据帧资源
  - **av_packet_free()** 释放数据包资源
  - **avcodec_close()** 关闭视频解码器的实例
  - **avcodec_free_context()** 释放视频解码器的实例
  - **avformat_close_input()** 关闭音视频文件
  - 释放数据帧资源/关闭输出流/关闭视频编码器的实例/释放视频编码器的实例/释放封装器的实例
  - *SaveJPGSwsFromVideo.cpp*

- 21.练习二十一：把视频解码后的视频帧保存为PNG图片
  - 操作流程与练习二十中类似
  - 这里要注意的是**save_png_file()** 时
    - **sws_getContext()** 设置的 *target_format*格式为：*AV_PIX_FMT_RGB24*
    - **avcodec_find_encoder()** 查找的编码器为*AV_CODEC_ID_PNG* 
      - ffmpeg中自带有对PNG图片编解码的PNG库，PNG编码器依赖于zlib库，所以在交叉编译时configure命令要加上*--enbale-zlib*
  - *SavePNGSwsFromVideo.cpp*

- 22.练习二十二：把视频解码后的视频帧保存为BMP图片
  - 操作流程与练习二十中类似
  - 这里要注意的是**save_bmp_file()** 时
    - **sws_getContext()** 设置的 *target_format*格式为：*AV_PIX_FMT_BGR24*
    - **sws_scale()** 转换器开始处理图像数据，把YUV图像转为RGB图像
    - **bmp_header** 设置bmp文件的头结构参数信息
    - **bmp_info** 设置bmp文件的信息结构参数信息
    - **fwrite()** 分别写入bmp文件头、bmp信息头
    - 把缓冲区的图像数据frame倒置过来，并写入文件中。
  - *SaveBMPSwsFromVideo.cpp*
  
- 23.练习二十三：把视频解码后的视频帧保存为GIF
  - 操作流程与练习二十中类似
  - 在**avcodec_find_encoder()** 时查找 *AV_CODEC_ID_GIF*的GIF编码器
  - **save_gif_file()** 时保留的是多个帧frame的数据
  - *SaveGifSwsOfVideo.cpp*

- 24.练习二十四：把图片转成视频
  - **open_input_file()** 打开第一个图片文件
    - **avformat_open_input()** 打开图像文件
    - **avformat_find_stream_info()** 查找图像文件中的流信息
    - **av_find_best_stream()** 找到视频流的索引
    - **avcodec_find_decoder()** 查找图像解码器
    - **avcodec_parameters_to_context()** 把视频流中的编解码参数复制给解码器的实例
    - **avcodec_open2()** 打开解码器的实例
  - **open_input_file()** 打开第二个图片文件
  - **open_output_file()** 打开输出文件
    - **avformat_alloc_output_context2()** 分配音视频文件的封装实例
    - **avio_open()** 打开输出流
    - **avcodec_find_encoder_by_name()** 使用libx264的编码器
    - **video_encode_ctx** 设置编码器的相关参数集
    - **avcodec_open2()** 打开编码器的实例
    - **avformat_new_stream()** 创建数据流
    - **avcodec_parameters_from_context()** 把编码器实例的参数复制给目标视频流
    - **avformat_write_header()** 写文件头
  - **init_sws_context()** 初始化第一个图像转换器的实例
    - **sws_getContext()** 分配图像转换器的实例，并分别指定来源和目标的宽度、高度、像素格式
    - **av_image_alloc()** 分配缓冲区空间，用于存放转换后的图像数据
  - **init_sws_context()** 初始化第二个图像转换器的实例
  - **av_packet_alloc()** 分配一个数据包
  - **av_frame_alloc()** 分配一个数据帧
  - **av_read_frame()** 轮询数据包
    - **recode_video()** 对视频帧重新编码
      - **avcodec_send_packet()** 把未解压的数据包发给解码器实例
      - **avcodec_receive_frame** 从解码器实例获取还原后的数据帧
      - **sws_scale()** 转换器开始处理图像数据，把RGB图像转为YUV图像
      - **output_video()** 给视频帧编码，并写入压缩后的视频包
        - **avcodec_send_frame()** 把原始的数据帧发给编码器实例
        - **avcodec_receive_packet()** 从编码器实例获取压缩后的数据包
        - **av_packet_rescale_ts()** 把数据包的时间戳从一个时间基转换为另一个时间基
        - **av_write_frame()** 往文件写入一个数据包
    - 释放数据帧资源/释放数据包资源/关闭输出流/关闭视频解码器的实例
       /释放视频解码器的实例/释放图像转换器的实例/释放封装器的实例/关闭音视频文件
  - *SaveImage2Video.cpp*

- 25.练习二十五：解码出音频帧保存为PCM
  - **avcodec_receive_frame** 从解码器实例获取还原后的数据帧
  - **av_sample_fmt_is_planar()** 判断指定音频采样格式是否为交错模式
    - **av_get_bytes_per_sample()** 平面模式的音频在存储时要改为交错模式
    - 非平面模式，直接写入文件

  
