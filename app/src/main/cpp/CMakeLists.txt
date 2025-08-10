cmake_minimum_required(VERSION 3.22.1)

project("ffmpegpractice")

set(distribution_DIR ${CMAKE_CURRENT_SOURCE_DIR})

message(${CMAKE_PROJECT_NAME})

set(jnilibs ${CMAKE_SOURCE_DIR}/jniLibs)

set(third-party-libs
       # ${jnilibs}/${ANDROID_ABI}/libassimp.so
       # ${jnilibs}/${ANDROID_ABI}/libfreetype.a
)

include_directories(
        includes

)

add_library(${CMAKE_PROJECT_NAME} SHARED
        # List C/C++ source files with relative paths to this CMakeLists.txt.
        native-lib.cpp)

target_link_libraries(${CMAKE_PROJECT_NAME}
        # List libraries link to the target library
        android
        log
        ${third-party-libs}
)