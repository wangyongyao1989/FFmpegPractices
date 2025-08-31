#!/bin/bash
# 用于编译android平台的脚本

# NDK所在目录
NDK_PATH=/home/wyy/Android/android-ndk-r27c
# 主机平台
HOST_PLATFORM=linux-x86_64
# minSdkVersion
API=24  # 提高到 API 24 以避免 fseeko64/ftello64 问题

TOOLCHAINS="$NDK_PATH/toolchains/llvm/prebuilt/$HOST_PLATFORM"
SYSROOT="$NDK_PATH/toolchains/llvm/prebuilt/$HOST_PLATFORM/sysroot"
# 生成 -fpic 与位置无关的代码
CFLAG="-D__ANDROID_API__=$API -Os -fPIC -DANDROID -D_FILE_OFFSET_BITS=64"
LDFLAG="-lc -lm -ldl -llog "

# 输出目录
PREFIX=$(pwd)/android
# 日志输出目录
CONFIG_LOG_PATH=${PREFIX}/log
# 公共配置
COMMON_OPTIONS=
# 交叉配置
CONFIGURATION=

# 创建必要的目录
mkdir -p ${PREFIX} ${CONFIG_LOG_PATH}

build() {
  APP_ABI=$1
  echo "======== > Start build $APP_ABI"

  # 重置变量
  EXTRA_CFLAGS="$CFLAG"
  EXTRA_LDFLAGS="$LDFLAG"
  EXTRA_OPTIONS=""
  PKG_CONFIG_AVAILABLE=false

  case ${APP_ABI} in
  armeabi-v7a)
    ARCH="arm"
    CPU="armv7-a"
    MARCH="armv7-a"
    TARGET=armv7a-linux-androideabi
    CC="$TOOLCHAINS/bin/$TARGET$API-clang"
    CXX="$TOOLCHAINS/bin/$TARGET$API-clang++"
    LD="$TOOLCHAINS/bin/$TARGET$API-clang"
    # 交叉编译工具前缀
    CROSS_PREFIX="$TOOLCHAINS/bin/arm-linux-androideabi-"

    # 设置pkg-config路径
    export PKG_CONFIG_PATH="/home/wyy/x264/x264/android/arm-v7a/lib/pkgconfig"
    echo "-------- > PKG_CONFIG_PATH=$PKG_CONFIG_PATH"

    # 指定X264的库
    X264_INCLUDE="/home/wyy/x264/x264/android/arm-v7a/include"
    X264_LIB="/home/wyy/x264/x264/android/arm-v7a/lib"
    echo "-------- > X264_INCLUDE=$X264_INCLUDE"
    echo "-------- > X264_LIB=$X264_LIB"

    # 检查x264库是否存在
    if [ ! -f "$X264_LIB/libx264.so" ]; then
        echo "错误: 找不到x264共享库文件在 $X264_LIB"
        echo "请先编译x264库或检查路径是否正确"
        exit 1
    fi

    # 检查x264.pc文件是否存在
    local x264_pc_file="$X264_LIB/pkgconfig/x264.pc"
    if [ -f "$x264_pc_file" ]; then
        echo "-------- > x264.pc found"
        # 尝试使用pkg-config获取正确的编译标志
        if pkg-config --exists x264; then
            X264_CFLAGS=$(pkg-config --cflags x264)
            X264_LIBS=$(pkg-config --libs x264)
            echo "-------- > pkg-config found x264 successfully"
            echo "-------- > x264 CFLAGS: $X264_CFLAGS"
            echo "-------- > x264 LIBS: $X264_LIBS"
            EXTRA_CFLAGS="$EXTRA_CFLAGS $X264_CFLAGS"
            EXTRA_LDFLAGS="$EXTRA_LDFLAGS $X264_LIBS"
            PKG_CONFIG_AVAILABLE=true
        else
            echo "-------- > WARNING: pkg-config cannot find x264, using manual flags"
            EXTRA_CFLAGS="$EXTRA_CFLAGS -I$X264_INCLUDE"
            EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L$X264_LIB -lx264"
        fi
    else
        echo "-------- > WARNING: x264.pc not found, using manual flags"
        EXTRA_CFLAGS="$EXTRA_CFLAGS -I$X264_INCLUDE"
        EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L$X264_LIB -lx264"
    fi

    # 添加架构特定标志
    EXTRA_CFLAGS="$EXTRA_CFLAGS -mfloat-abi=softfp -mfpu=neon -marm -march=$MARCH"
    EXTRA_OPTIONS="--enable-neon --cpu=$CPU"
    ;;

  arm64-v8a)
    ARCH="aarch64"
    CPU="armv8-a"
    TARGET=aarch64-linux-android
    CC="$TOOLCHAINS/bin/$TARGET$API-clang"
    CXX="$TOOLCHAINS/bin/$TARGET$API-clang++"
    LD="$TOOLCHAINS/bin/$TARGET$API-clang"
    CROSS_PREFIX="$TOOLCHAINS/bin/aarch64-linux-android-"

    # 设置pkg-config路径
    export PKG_CONFIG_PATH="/home/wyy/x264/x264/android/arm64-v8a/lib/pkgconfig"

    # 指定X264的库
    X264_INCLUDE="/home/wyy/x264/x264/android/arm64-v8a/include"
    X264_LIB="/home/wyy/x264/x264/android/arm64-v8a/lib"

    # 检查x264库是否存在
    if [ ! -f "$X264_LIB/libx264.so" ]; then
        echo "错误: 找不到x264共享库文件在 $X264_LIB"
        exit 1
    fi

    # 手动添加x264标志
    EXTRA_CFLAGS="$EXTRA_CFLAGS -I$X264_INCLUDE"
    EXTRA_LDFLAGS="$EXTRA_LDFLAGS -L$X264_LIB -lx264"
    ;;

  x86)
    ARCH="x86"
    CPU="i686"
    TARGET=i686-linux-android
    CC="$TOOLCHAINS/bin/$TARGET$API-clang"
    CXX="$TOOLCHAINS/bin/$TARGET$API-clang++"
    LD="$TOOLCHAINS/bin/$TARGET$API-clang"
    CROSS_PREFIX="$TOOLCHAINS/bin/i686-linux-android-"

    # 禁用不支持的选项
    EXTRA_OPTIONS="--disable-neon --disable-asm"
    ;;

  x86_64)
    ARCH="x86_64"
    CPU="x86_64"
    TARGET=x86_64-linux-android
    CC="$TOOLCHAINS/bin/$TARGET$API-clang"
    CXX="$TOOLCHAINS/bin/$TARGET$API-clang++"
    LD="$TOOLCHAINS/bin/$TARGET$API-clang"
    CROSS_PREFIX="$TOOLCHAINS/bin/x86_64-linux-android-"

    # 禁用不支持的选项
    EXTRA_OPTIONS="--disable-neon"
    ;;

  esac

  echo "-------- > Start clean workspace"
  make clean > /dev/null 2>&1

  echo "-------- > Start build configuration"
  CONFIGURATION="$COMMON_OPTIONS"
  CONFIGURATION="$CONFIGURATION --logfile=$CONFIG_LOG_PATH/config_$APP_ABI.log"
  CONFIGURATION="$CONFIGURATION --prefix=$PREFIX"
  CONFIGURATION="$CONFIGURATION --libdir=$PREFIX/libs/$APP_ABI"
  CONFIGURATION="$CONFIGURATION --incdir=$PREFIX/includes/$APP_ABI"
  CONFIGURATION="$CONFIGURATION --pkgconfigdir=$PREFIX/pkgconfig/$APP_ABI"
  CONFIGURATION="$CONFIGURATION --cross-prefix=$CROSS_PREFIX"
  CONFIGURATION="$CONFIGURATION --arch=$ARCH"
  CONFIGURATION="$CONFIGURATION --sysroot=$SYSROOT"
  CONFIGURATION="$CONFIGURATION --cc=$CC"
  CONFIGURATION="$CONFIGURATION --cxx=$CXX"
  CONFIGURATION="$CONFIGURATION --ld=$LD"
  #libx264
  CONFIGURATION="$CONFIGURATION --enable-libx264"
  CONFIGURATION="$CONFIGURATION --enable-encoder=libx264"
  # nm 和 strip
  CONFIGURATION="$CONFIGURATION --nm=$TOOLCHAINS/bin/llvm-nm"
  CONFIGURATION="$CONFIGURATION --strip=$TOOLCHAINS/bin/llvm-strip"
  CONFIGURATION="$CONFIGURATION $EXTRA_OPTIONS"



  echo "-------- > Start config makefile with $CONFIGURATION"
  echo "-------- > Extra CFLAGS: $EXTRA_CFLAGS"
  echo "-------- > Extra LDFLAGS: $EXTRA_LDFLAGS"

  ./configure ${CONFIGURATION} \
    --extra-cflags="$EXTRA_CFLAGS" \
    --extra-ldflags="$EXTRA_LDFLAGS" \
    --pkg-config=$(which pkg-config) 2>&1 | tee $CONFIG_LOG_PATH/configure_$APP_ABI.log

  echo "-------- > Start make $APP_ABI with -j4"
  make -j4

  echo "-------- > Start install $APP_ABI"
  make install

  echo "++++++++ > make and install $APP_ABI complete."
  return 0
}

build_all() {
  #配置开源协议声明
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-gpl"
  #目标android平台
  COMMON_OPTIONS="$COMMON_OPTIONS --target-os=android"
  #取消默认的静态库
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-static"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-shared"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-protocols"
  #开启交叉编译
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-cross-compile"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-optimizations"
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-debug"
  #尽可能小
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-small"
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-doc"
  #不要命令（执行文件）
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-programs"  # do not build command line programs
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-ffmpeg"    # disable ffmpeg build
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-ffplay"    # disable ffplay build
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-ffprobe"   # disable ffprobe build
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-symver"
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-network"
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-x86asm"
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-vulkan"

  # 有条件地禁用asm，某些架构可能不支持
  COMMON_OPTIONS="$COMMON_OPTIONS --disable-asm"

  #启用
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-pthreads"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-mediacodec"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-jni"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-zlib"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-pic"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-muxer=flv"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=h264"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=mpeg4"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=mjpeg"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=png"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=vorbis"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=opus"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-decoder=flac"

  #启用编码器
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-encoder=h264"

  #libx264
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-libx264"
  COMMON_OPTIONS="$COMMON_OPTIONS --enable-encoder=libx264"

  echo "COMMON_OPTIONS=$COMMON_OPTIONS"
  echo "PREFIX=$PREFIX"
  echo "CONFIG_LOG_PATH=$CONFIG_LOG_PATH"

  # 构建所有支持的架构
  build "armeabi-v7a" || echo "Failed to build armeabi-v7a"
  build "arm64-v8a" || echo "Failed to build arm64-v8a"
  # build "x86" || echo "Failed to build x86"
  # build "x86_64" || echo "Failed to build x86_64"
}

echo "-------- Start --------"
build_all
echo "-------- End --------"