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
#include <atomic>

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

    // 清除队列中的所有元素
    void clear() {
        lock_guard<mutex> lock(mutex_);
        while (!queue_.empty()) {
            // 如果 T 是指针类型，可能需要特殊处理来释放内存
            // 这里只是简单地弹出元素，让元素自动析构
            queue_.pop();
        }
        // 清除后通知可能正在等待的线程
        condition_.notify_all();
    }

    // 清除队列并返回被清除的元素数量
    size_t clear_and_count() {
        lock_guard<mutex> lock(mutex_);
        size_t count = queue_.size();
        while (!queue_.empty()) {
            queue_.pop();
        }
        condition_.notify_all();
        return count;
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

    // 带超时的出队操作
    template<typename Rep, typename Period>
    bool pop(T& value, const chrono::duration<Rep, Period>& timeout) {
        unique_lock<mutex> lock(mutex_);
        if (!condition_.wait_for(lock, timeout, [this] {
            return stop_flag_.load() || !queue_.empty();
        })) {
            return false; // 超时
        }

        if (stop_flag_.load() && queue_.empty()) {
            return false;
        }

        value = move(queue_.front());
        queue_.pop();
        return true;
    }

    // 检查队列是否已停止
    bool is_stopped() const {
        return stop_flag_.load();
    }

    // 重新启动队列（在停止后）
    void restart() {
        stop_flag_.store(false);
    }

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