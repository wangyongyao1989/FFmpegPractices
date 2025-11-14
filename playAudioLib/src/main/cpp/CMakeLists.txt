cmake_minimum_required(VERSION 3.22.1)

project("playaudiolib")

set(distribution_DIR ${CMAKE_CURRENT_SOURCE_DIR})

message(${CMAKE_PROJECT_NAME})

set(jnilibs ${CMAKE_SOURCE_DIR}/jniLibs)

set(third-party-libs
        ${jnilibs}/${ANDROID_ABI}/libavcodec.so
        ${jnilibs}/${ANDROID_ABI}/libavdevice.so
        ${jnilibs}/${ANDROID_ABI}/libavfilter.so
        ${jnilibs}/${ANDROID_ABI}/libavformat.so
        ${jnilibs}/${ANDROID_ABI}/libavutil.so
        ${jnilibs}/${ANDROID_ABI}/libpostproc.so
        ${jnilibs}/${ANDROID_ABI}/libswresample.so
        ${jnilibs}/${ANDROID_ABI}/libswscale.so
)

include_directories(
        includes
)

add_library(
        ${CMAKE_PROJECT_NAME}
        SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        PlayAudioJniCall.cpp
        AndroidThreadManager.cpp
        PlayAudioTrack.cpp
        OpenslHelper.cpp
        FFmpegOpenSLPlayer.cpp

)

target_link_libraries(
        ${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        mediandk
        log
        #EGL为OpenGL ES与系统底层之间的接口层，OpenSLES用于播放音频；
        OpenSLES
        ${third-party-libs}

)