//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/2.
//

#ifndef FFMPEGPRACTICE_THREADPOOLUTILS_H
#define FFMPEGPRACTICE_THREADPOOLUTILS_H

#include <iostream>
#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <memory>
#include <filesystem>
#include <chrono>
#include <cstring>


using namespace std;
namespace fs = std::filesystem;


class ThreadPoolUtils {

public:
    // 任务类型定义
    using Task = std::function<void()>;

    // 构造函数，创建指定数量的工作线程
    explicit ThreadPoolUtils(size_t numThreads);

    // 析构函数，等待所有线程完成
    ~ThreadPoolUtils();

    // 添加任务到线程池
    template<class F>
    void enqueue(F &&f);

    // 获取任务队列大小
    size_t getQueueSize();

    // 获取工作线程数量
    size_t getThreadCount();

private:
    std::vector<std::thread> workers;
    std::queue<Task> tasks;

    std::mutex queueMutex;
    std::condition_variable condition;
    bool stop;
};

// 模板函数必须在头文件中实现
template<class F>
void ThreadPoolUtils::enqueue(F &&f) {
    {
        std::unique_lock<std::mutex> lock(queueMutex);
        tasks.emplace(std::forward<F>(f));
    }
    condition.notify_one();
}


#endif //FFMPEGPRACTICE_THREADPOOLUTILS_H
