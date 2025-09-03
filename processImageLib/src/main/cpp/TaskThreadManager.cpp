//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/2.
//

#include "TaskThreadManager.h"
#include <iostream>

// 构造函数
TaskThreadManager::TaskThreadManager(size_t threadCount)
        : stopRequested(false), activeCount(0) {

    if (threadCount == 0) threadCount = 1;

    workers.reserve(threadCount);
    for (size_t i = 0; i < threadCount; ++i) {
        workers.emplace_back(&TaskThreadManager::workerThread, this);
    }
}

// 析构函数
TaskThreadManager::~TaskThreadManager() {
    stop();
    waitAll();
}

// 工作线程函数
void TaskThreadManager::workerThread() {
    while (!stopRequested) {
        function<void()> task;

        {
            unique_lock<mutex> lock(queueMutex);

            // 等待任务或停止请求
            condition.wait(lock, [this] {
                return stopRequested || !tasks.empty();
            });

            // 如果请求停止且没有任务，退出
            if (stopRequested && tasks.empty()) {
                return;
            }

            // 获取任务
            task = move(tasks.front());
            tasks.pop();
        }

        // 执行任务
        activeCount++;
        try {
            task();
        } catch (const exception &e) {
            cerr << "Exception in task: " << e.what() << endl;
        } catch (...) {
            cerr << "Unknown exception in task" << endl;
        }
        activeCount--;
    }
}

// 等待所有任务完成
void TaskThreadManager::waitAll() {
    while (true) {
        {
            unique_lock<mutex> lock(queueMutex);
            if (tasks.empty() && activeCount == 0) {
                break;
            }
        }
        this_thread::sleep_for(chrono::milliseconds(10));
    }
}

// 停止所有工作线程
void TaskThreadManager::stop() {
    stopRequested = true;
    condition.notify_all();
}

// 获取任务队列大小
size_t TaskThreadManager::taskCount() const {
    unique_lock<mutex> lock(queueMutex);
    return tasks.size();
}

// 获取活动线程数量
size_t TaskThreadManager::activeThreads() const {
    return activeCount;
}