//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/2.
//

#ifndef FFMPEGPRACTICE_TASKTHREADMANAGER_H
#define FFMPEGPRACTICE_TASKTHREADMANAGER_H

#include <vector>
#include <queue>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <functional>
#include <atomic>
#include <future>
#include <memory>

using namespace std;

class TaskThreadManager {
public:
    // 构造函数
    TaskThreadManager(size_t threadCount = thread::hardware_concurrency());

    // 析构函数
    ~TaskThreadManager();

    // 添加任务到队列
    template<typename F, typename... Args>
    auto addTask(F &&f, Args &&... args) -> future<typename result_of<F(Args...)>::type>;

    // 等待所有任务完成
    void waitAll();

    // 停止所有工作线程
    void stop();

    // 获取任务队列大小
    size_t taskCount() const;

    // 获取活动线程数量
    size_t activeThreads() const;

private:
    // 工作线程函数
    void workerThread();

    // 线程池
    vector<thread> workers;

    // 任务队列
    queue<function<void()>> tasks;

    // 同步
    mutable mutex queueMutex;
    condition_variable condition;

    // 控制标志
    atomic<bool> stopRequested;
    atomic<size_t> activeCount;
};

// 模板函数实现必须在头文件中
template<typename F, typename... Args>
auto
TaskThreadManager::addTask(F &&f, Args &&... args) -> future<typename result_of<F(Args...)>::type> {
    using return_type = typename result_of<F(Args...)>::type;

    // 创建packaged_task来包装函数和参数
    auto task = make_shared<packaged_task<return_type()>>(
            bind(forward<F>(f), forward<Args>(args)...)
    );

    // 获取future以便获取返回值
    future<return_type> res = task->get_future();

    {
        unique_lock<mutex> lock(queueMutex);

        // 不允许在停止后添加任务
        if (stopRequested) {
            throw runtime_error("Cannot add task to stopped TaskThreadManager");
        }

        // 将任务添加到队列
        tasks.emplace([task]() { (*task)(); });
    }

    // 通知一个等待的线程
    condition.notify_one();
    return res;
}

#endif // TASK_THREAD_MANAGER_H