//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/2.
//

#include "includes/ThreadPoolUtils.h"

ThreadPoolUtils::ThreadPoolUtils(size_t numThreads) : stop(false) {
    for (size_t i = 0; i < numThreads; ++i) {
        workers.emplace_back([this] {
            while (true) {
                Task task;
                {
                    unique_lock<mutex> lock(this->queueMutex);
                    this->condition.wait(lock, [this] {
                        return this->stop || !this->tasks.empty();
                    });

                    if (this->stop && this->tasks.empty()) {
                        return;
                    }

                    task = move(this->tasks.front());
                    this->tasks.pop();
                }

                task();
            }
        });
    }
}

ThreadPoolUtils::~ThreadPoolUtils() {
    {
        unique_lock<mutex> lock(queueMutex);
        stop = true;
    }

    condition.notify_all();

    for (thread &worker: workers) {
        if (worker.joinable()) {
            worker.join();
        }
    }
}


size_t ThreadPoolUtils::getQueueSize() {
    unique_lock<mutex> lock(queueMutex);
    return tasks.size();
}

size_t ThreadPoolUtils::getThreadCount() {
    return workers.size();
}