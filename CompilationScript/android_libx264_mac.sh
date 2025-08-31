#!/bin/bash
# macOS平台下用于android平台NDK为android-ndk-r27c交叉编译X264的脚本

# NDK所在目录
export NDK=/Users/wangyao/Library/Android/sdk/ndk/25.1.8937393 # tag1
export TOOLCHAIN=$NDK/toolchains/llvm/prebuilt/darwin-x86_64
export SYSROOT=$NDK/toolchains/llvm/prebuilt/darwin-x86_64/sysroot

export ANDROID_API=24
export AR=$TOOLCHAIN/bin/llvm-ar

export LD=$TOOLCHAIN/bin/ld
export RANLIB=$TOOLCHAIN/bin/llvm-ranlib
export STRIP=$TOOLCHAIN/bin/llvm-strip
export NM=$TOOLCHAIN/bin/llvm-nm
#export STRINGS=$TOOLCHAIN/llvm-strings


build_armv7(){
  echo "-------- > Start install build_armv7"
  export CC=$TOOLCHAIN/bin/armv7a-linux-androideabi$ANDROID_API-clang
  export CXX=$TOOLCHAIN/bin/armv7a-linux-androideabi$ANDROID_API-clang++

    #编译选项
  EXTRA_CFLAGS="-march=armv7-a -D__ANDROID__"


  PREFIX=`pwd`/android/arm-v7a

  # 配置和编译
  ./configure \
  --host=armv7a-linux-androideabi \
  --sysroot=${SYSROOT} \
  --prefix=${PREFIX} \
  --enable-shared \
  --enable-static \
  --disable-asm \
  --chroma-format=all \
  --enable-pic \
  --enable-strip \
  --disable-cli \
  --disable-win32thread \
  --disable-avs \
  --disable-swscale \
  --disable-lavf \
  --disable-ffms \
  --disable-gpac \
  --disable-lsmash \
  --extra-cflags="-Os -fpic ${EXTRA_CFLAGS}" \
  --extra-ldflags="" \

  make clean
  make -j4
  make install

}

build_armv8(){

  echo "-------- > Start install build_armv8"

  export CC=$TOOLCHAIN/bin/aarch64-linux-android$ANDROID_API-clang
  export CXX=$TOOLCHAIN/bin/aarch64-linux-android$ANDROID_API-clang++

  #编译选项
EXTRA_CFLAGS="-march=armv8-a -D__ANDROID__"

  PREFIX=`pwd`/android/arm64-v8a
  # 配置和编译
  ./configure \
  --host=aarch64-linux-android \
  --sysroot=${SYSROOT} \
  --prefix=${PREFIX} \
  --enable-shared \
  --enable-static \
  --disable-asm \
  --chroma-format=all \
  --enable-pic \
  --enable-strip \
  --disable-cli \
  --disable-win32thread \
  --disable-avs \
  --disable-swscale \
  --disable-lavf \
  --disable-ffms \
  --disable-gpac \
  --disable-lsmash \
  --extra-cflags="-Os -fpic ${EXTRA_CFLAGS}" \
  --extra-ldflags="" \

  make clean
  make -j4
  make install

}


build_x86(){

  echo "-------- > Start install build_x86"

  export CC=$TOOLCHAIN/bin/i686-linux-android$ANDROID_API-clang
  export CXX=$TOOLCHAIN/bin/i686-linux-android$ANDROID_API-clang++

  #编译选项
  EXTRA_CFLAGS="-D__ANDROID__"

  PREFIX=`pwd`/android/x86
  # 配置和编译
  ./configure \
  --host=i686-linux-android \
  --sysroot=${SYSROOT} \
  --prefix=${PREFIX} \
  --enable-shared \
  --enable-static \
  --chroma-format=all \
  --disable-asm \
  --enable-pic \
  --enable-strip \
  --disable-cli \
  --disable-win32thread \
  --disable-avs \
  --disable-swscale \
  --disable-lavf \
  --disable-ffms \
  --disable-gpac \
  --disable-lsmash \
  --extra-cflags="-Os -fpic ${EXTRA_CFLAGS}" \
  --extra-ldflags="" \

  make clean
  make -j4
  make install

}


build_x86_64(){

  echo "-------- > Start install build_x86_64"
  export CC=$TOOLCHAIN/bin/x86_64-linux-android$ANDROID_API-clang
  export CXX=$TOOLCHAIN/bin/x86_64-linux-android$ANDROID_API-clang++

  #编译选项
  EXTRA_CFLAGS="-D__ANDROID__"

  PREFIX=`pwd`/android/x86_64
  # 配置和编译
  ./configure \
  --host=x86_64-linux-android \
  --sysroot=${SYSROOT} \
  --prefix=${PREFIX} \
  --enable-shared \
  --enable-static \
  --disable-asm \
  --chroma-format=all \
  --enable-pic \
  --enable-strip \
  --disable-cli \
  --disable-win32thread \
  --disable-avs \
  --disable-swscale \
  --disable-lavf \
  --disable-ffms \
  --disable-gpac \
  --disable-lsmash \
  --extra-cflags="-Os -fpic ${EXTRA_CFLAGS}" \
  --extra-ldflags="" \

  make clean
  make -j4
  make install

}

build_armv7
build_armv8
build_x86
build_x86_64

