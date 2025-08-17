//
// Created by wangyao on 2025/8/17.
//

#ifndef FFMPEGPRACTICE_COPYMEIDAFILE_H
#define FFMPEGPRACTICE_COPYMEIDAFILE_H

#include "BasicCommon.h"
#include "string"

using namespace std;

class CopyMeidaFile {

private:
    string copyInfo;
    AVFormatContext *in_fmt_ctx = nullptr; // 输入文件的封装器实例

public:
    CopyMeidaFile();

    ~CopyMeidaFile();

    string copyMediaFile(const char *srcPath, const char *destPath);

};


#endif //FFMPEGPRACTICE_COPYMEIDAFILE_H
