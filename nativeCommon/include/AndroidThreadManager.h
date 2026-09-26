//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/3.
//

#ifndef ANDROID_THREAD_MANAGER_H
#define ANDROID_THREAD_MANAGER_H

#include <jni.h>
#include <pthread.h>
#include <unordered_map>
#include <string>
#include <functional>
#include <atomic>
#include <mutex>
#include <condition_variable>
#include <queue>
#include <vector>
#include <memory>
#include "LogUtils.h"

// 线程优先级枚举
enum ThreadPriority {
    PRIORITY_IDLE = 1,      // 最低优先级
    PRIORITY_LOW = 2,
    PRIORITY_BELOW_NORMAL = 3,
    PRIORITY_NORMAL = 4,    // 默认优先级
    PRIORITY_ABOVE_NORMAL = 5,
    PRIORITY_HIGH = 6,
    PRIORITY_REALTIME = 7   // 最高优先级（谨慎使用）
};

// 线程状态枚举
enum ThreadState {
    THREAD_STATE_IDLE,      // 空闲（已创建但未运行）
    THREAD_STATE_RUNNING,   // 运行中
    THREAD_STATE_PAUSED,    // 已暂停
    THREAD_STATE_STOPPING,  // 正在停止
    THREAD_STATE_STOPPED,   // 已停止
    THREAD_STATE_COMPLETED, // 已完成
    THREAD_STATE_ERROR      // 错误状态
};

// 线程任务回调函数类型
using ThreadTask = std::function<void()>;

// 线程池配置
struct ThreadPoolConfig {
    size_t minThreads;      // 最小线程数
    size_t maxThreads;      // 最大线程数
    size_t idleTimeoutMs;   // 空闲线程超时时间（毫秒）
    size_t queueSize;       // 任务队列大小（0表示无限制）
};

class AndroidThreadManager {
public:
    // 构造函数/析构函数
    AndroidThreadManager(JavaVM *jvm = nullptr);

    virtual ~AndroidThreadManager();

    // 删除拷贝构造函数和赋值运算符
    AndroidThreadManager(const AndroidThreadManager &) = delete;

    AndroidThreadManager &operator=(const AndroidThreadManager &) = delete;

    // 单线程管理方法
    bool createThread(const std::string &threadName, ThreadTask task,
                      ThreadPriority priority = PRIORITY_NORMAL);

    bool pauseThread(const std::string &threadName);

    bool resumeThread(const std::string &threadName);

    bool stopThread(const std::string &threadName, bool waitForExit = true);

    bool joinThread(const std::string &threadName);

    ThreadState getThreadState(const std::string &threadName);

    bool setThreadPriority(const std::string &threadName, ThreadPriority priority);

    // 线程池管理方法
    bool initThreadPool(const ThreadPoolConfig &config);

    bool submitTask(const std::string &taskName, ThreadTask task,
                    ThreadPriority priority = PRIORITY_NORMAL);

    bool cancelTask(const std::string &taskName);

    void shutdownThreadPool(bool waitForCompletion = true);

    // 工具方法
    std::unordered_map<std::string, ThreadState> getAllThreadStates();

    void cleanupCompletedThreads();

    size_t getActiveThreadCount();

    size_t getPendingTaskCount();

    // JVM相关方法
    void setJavaVM(JavaVM *jvm) { m_javaVM = jvm; }

    JavaVM *getJavaVM() const { return m_javaVM; }

    // 静态工具方法
    static bool setCurrentThreadName(const std::string &name);

    static bool setCurrentThreadPriority(ThreadPriority priority);

    static int getCurrentThreadId();

private:
    // 线程内部数据结构
    struct ThreadData {
        pthread_t thread;
        std::string name;
        ThreadTask task;
        ThreadPriority priority;
        ThreadState state;
        std::mutex mutex;
        std::condition_variable condition;
        bool shouldResume;
        std::atomic<bool> shouldStop;
        JavaVM *jvm;
        pid_t tid; // 线程ID

        ThreadData() :
                priority(PRIORITY_NORMAL),
                state(THREAD_STATE_IDLE),
                shouldResume(false),
                shouldStop(false),
                jvm(nullptr),
                tid(-1) {}
    };

    // 线程池任务结构
    struct PoolTask {
        std::string name;
        ThreadTask task;
        ThreadPriority priority;

        PoolTask(const std::string &n, ThreadTask t, ThreadPriority p) :
                name(n), task(t), priority(p) {}
    };

    // 线程池工作线程数据
    struct PoolWorker {
        pthread_t thread;
        std::atomic<bool> shouldStop;
        AndroidThreadManager *manager;
        pid_t tid;

        PoolWorker(AndroidThreadManager *m) :
                shouldStop(false), manager(m), tid(-1) {}
    };

    // 线程执行函数
    static void *threadFunction(void *arg);

    // 线程池工作函数
    static void *threadPoolWorker(void *arg);

    // JVM附加/分离辅助函数
    static JNIEnv *attachJVM(JavaVM *jvm, const std::string &threadName);

    static void detachJVM(JavaVM *jvm);

    // 成员变量
    std::unordered_map<std::string, std::shared_ptr<ThreadData>> m_threads;
    std::mutex m_threadsMutex;

    // 线程池相关成员
    std::vector<std::unique_ptr<PoolWorker>> m_poolWorkers;
    std::queue<PoolTask> m_taskQueue;
    std::mutex m_poolMutex;
    std::condition_variable m_poolCondition;
    ThreadPoolConfig m_poolConfig;
    std::atomic<bool> m_poolShutdown;
    std::atomic<size_t> m_activeWorkers;

    JavaVM *m_javaVM;
    std::atomic<bool> m_isDestroying;
};

#endif // ANDROID_THREAD_MANAGER_H
