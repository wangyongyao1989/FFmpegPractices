//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/11/21.
//

#ifndef FFMPEGPRACTICE_THREADSAFEQUEUE_H
#define FFMPEGPRACTICE_THREADSAFEQUEUE_H

#include <queue>
#include <mutex>
#include <condition_variable>
#include <memory>
#include <chrono>

using namespace std;

template<typename T>
class ThreadSafeQueue {
private:
    mutable mutex mutex_;
    queue<T> queue_;
    condition_variable condition_;
    atomic<bool> stop_flag_{false};

public:
    ThreadSafeQueue() = default;

    // 禁止拷贝
    ThreadSafeQueue(const ThreadSafeQueue&) = delete;
    ThreadSafeQueue& operator=(const ThreadSafeQueue&) = delete;

    // 析构函数：设置停止标志并通知所有等待线程
    ~ThreadSafeQueue() {
        stop();
    }

    // 停止队列，通知所有等待线程
    void stop() {
        stop_flag_.store(true);
        condition_.notify_all();  // 重要：唤醒所有等待的线程
    }

    // 入队操作
    bool push(T value) {
        if (stop_flag_.load()) {
            return false;  // 队列已停止，拒绝新元素
        }

        lock_guard<mutex> lock(mutex_);
        if (stop_flag_.load()) {
            return false;
        }

        queue_.push(move(value));
        condition_.notify_one();
        return true;
    }

    // 出队操作（阻塞直到有元素或停止）
    bool pop(T& value) {
        unique_lock<mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return stop_flag_.load() || !queue_.empty();
        });

        if (stop_flag_.load() && queue_.empty()) {
            return false;  // 已停止且队列为空
        }

        value = move(queue_.front());
        queue_.pop();
        return true;
    }

    // 返回智能指针的出队操作
    shared_ptr<T> pop() {
        unique_lock<mutex> lock(mutex_);
        condition_.wait(lock, [this] {
            return stop_flag_.load() || !queue_.empty();
        });

        if (stop_flag_.load() && queue_.empty()) {
            return nullptr;
        }

        auto result = make_shared<T>(move(queue_.front()));
        queue_.pop();
        return result;
    }

    // 检查队列是否已停止
    bool is_stopped() const {
        return stop_flag_.load();
    }

    // 其他方法保持不变...
    bool empty() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.empty();
    }

    size_t size() const {
        lock_guard<mutex> lock(mutex_);
        return queue_.size();
    }
};

#endif //FFMPEGPRACTICE_THREADSAFEQUEUE_H
