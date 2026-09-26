//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/9/3.
//


#include "AndroidThreadManager.h"
#include <android/log.h>
#include <sys/types.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <sched.h>
#include <time.h>
#include <linux/resource.h>
#include <sys/resource.h>


AndroidThreadManager::AndroidThreadManager(JavaVM *jvm)
        : m_javaVM(jvm),
          m_isDestroying(false),
          m_poolShutdown(false),
          m_activeWorkers(0) {
    // 设置默认线程池配置
    m_poolConfig.minThreads = 2;
    m_poolConfig.maxThreads = 4;
    m_poolConfig.idleTimeoutMs = 30000; // 30秒
    m_poolConfig.queueSize = 100;
}

AndroidThreadManager::~AndroidThreadManager() {
    m_isDestroying.store(true);

    // 关闭线程池
    shutdownThreadPool(false);

    // 停止所有线程
    std::unique_lock<std::mutex> lock(m_threadsMutex);
    for (auto &pair: m_threads) {
        auto &data = pair.second;

        {
            std::unique_lock<std::mutex> threadLock(data->mutex);
            data->shouldStop = true;
            data->state = THREAD_STATE_STOPPING;
            data->condition.notify_all();
        }

        // 等待线程结束
        if (data->state == THREAD_STATE_RUNNING || data->state == THREAD_STATE_PAUSED) {
            pthread_join(data->thread, nullptr);
        }
    }
    m_threads.clear();
}

bool AndroidThreadManager::createThread(const std::string &threadName, ThreadTask task,
                                        ThreadPriority priority) {
    std::unique_lock<std::mutex> lock(m_threadsMutex);

    if (m_threads.find(threadName) != m_threads.end()) {
        LOGE("Thread with name %s already exists", threadName.c_str());
        return false;
    }

    auto data = std::make_shared<ThreadData>();
    data->name = threadName;
    data->task = task;
    data->priority = priority;
    data->state = THREAD_STATE_IDLE;
    data->shouldResume = false;
    data->shouldStop = false;
    data->jvm = m_javaVM;

    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

    if (pthread_create(&data->thread, &attr, threadFunction, data.get()) != 0) {
        LOGE("Failed to create thread %s", threadName.c_str());
        pthread_attr_destroy(&attr);
        return false;
    }

    m_threads[threadName] = data;
    pthread_attr_destroy(&attr);
    return true;
}

bool AndroidThreadManager::pauseThread(const std::string &threadName) {
    std::unique_lock<std::mutex> lock(m_threadsMutex);

    auto it = m_threads.find(threadName);
    if (it == m_threads.end()) {
        LOGE("Thread %s not found", threadName.c_str());
        return false;
    }

    auto &data = it->second;
    std::unique_lock<std::mutex> threadLock(data->mutex);

    if (data->state == THREAD_STATE_RUNNING) {
        data->state = THREAD_STATE_PAUSED;
        LOGI("Thread %s paused", threadName.c_str());
        return true;
    }

    LOGW("Cannot pause thread %s, current state: %d", threadName.c_str(), data->state);
    return false;
}

bool AndroidThreadManager::resumeThread(const std::string &threadName) {
    std::unique_lock<std::mutex> lock(m_threadsMutex);

    auto it = m_threads.find(threadName);
    if (it == m_threads.end()) {
        LOGE("Thread %s not found", threadName.c_str());
        return false;
    }

    auto &data = it->second;
    std::unique_lock<std::mutex> threadLock(data->mutex);

    if (data->state == THREAD_STATE_PAUSED) {
        data->state = THREAD_STATE_RUNNING;
        data->shouldResume = true;
        data->condition.notify_all();
        LOGI("Thread %s resumed", threadName.c_str());
        return true;
    }

    LOGW("Cannot resume thread %s, current state: %d", threadName.c_str(), data->state);
    return false;
}

bool AndroidThreadManager::stopThread(const std::string &threadName, bool waitForExit) {
    std::shared_ptr<ThreadData> data;

    {
        std::unique_lock<std::mutex> lock(m_threadsMutex);
        auto it = m_threads.find(threadName);
        if (it == m_threads.end()) {
            LOGE("Thread %s not found", threadName.c_str());
            return false;
        }
        data = it->second;
    }

    {
        std::unique_lock<std::mutex> threadLock(data->mutex);
        data->shouldStop = true;
        data->state = THREAD_STATE_STOPPING;
        data->condition.notify_all();
    }

    if (waitForExit) {
        pthread_join(data->thread, nullptr);

        std::unique_lock<std::mutex> lock(m_threadsMutex);
        m_threads.erase(threadName);
    }

    LOGI("Thread %s stopped", threadName.c_str());
    return true;
}

bool AndroidThreadManager::joinThread(const std::string &threadName) {
    std::shared_ptr<ThreadData> data;

    {
        std::unique_lock<std::mutex> lock(m_threadsMutex);
        auto it = m_threads.find(threadName);
        if (it == m_threads.end()) {
            LOGE("Thread %s not found", threadName.c_str());
            return false;
        }
        data = it->second;
    }

    if (pthread_join(data->thread, nullptr) != 0) {
        LOGE("Failed to join thread %s", threadName.c_str());
        return false;
    }

    return true;
}

ThreadState AndroidThreadManager::getThreadState(const std::string &threadName) {
    std::unique_lock<std::mutex> lock(m_threadsMutex);

    auto it = m_threads.find(threadName);
    if (it == m_threads.end()) {
        LOGE("Thread %s not found", threadName.c_str());
        return THREAD_STATE_STOPPED;
    }

    std::unique_lock<std::mutex> threadLock(it->second->mutex);
    return it->second->state;
}

bool
AndroidThreadManager::setThreadPriority(const std::string &threadName, ThreadPriority priority) {
    std::unique_lock<std::mutex> lock(m_threadsMutex);

    auto it = m_threads.find(threadName);
    if (it == m_threads.end()) {
        LOGE("Thread %s not found", threadName.c_str());
        return false;
    }

    auto &data = it->second;
    std::unique_lock<std::mutex> threadLock(data->mutex);
    data->priority = priority;

    // 如果线程正在运行，实时设置优先级
    if (data->state == THREAD_STATE_RUNNING) {
        // 使用pthread_setschedprio设置实时优先级
        int policy;
        struct sched_param param;
        pthread_getschedparam(data->thread, &policy, &param);

        // 转换为系统优先级值
        int sysPriority;
        switch (priority) {
            case PRIORITY_IDLE:
                sysPriority = 1;
                break;
            case PRIORITY_LOW:
                sysPriority = 10;
                break;
            case PRIORITY_BELOW_NORMAL:
                sysPriority = 15;
                break;
            case PRIORITY_NORMAL:
                sysPriority = 20;
                break;
            case PRIORITY_ABOVE_NORMAL:
                sysPriority = 25;
                break;
            case PRIORITY_HIGH:
                sysPriority = 30;
                break;
            case PRIORITY_REALTIME:
                sysPriority = 40;
                break;
            default:
                sysPriority = 20;
        }

        param.sched_priority = sysPriority;
        if (pthread_setschedparam(data->thread, SCHED_OTHER, &param) != 0) {
            LOGW("Failed to set thread priority for %s", threadName.c_str());
            return false;
        }
    }

    return true;
}

bool AndroidThreadManager::initThreadPool(const ThreadPoolConfig &config) {
    std::unique_lock<std::mutex> lock(m_poolMutex);

    if (!m_poolWorkers.empty()) {
        LOGE("Thread pool already initialized");
        return false;
    }

    m_poolConfig = config;
    m_poolShutdown = false;

    // 创建最小数量的工作线程
    for (size_t i = 0; i < m_poolConfig.minThreads; ++i) {
        auto worker = std::make_unique<PoolWorker>(this);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if (pthread_create(&worker->thread, &attr, threadPoolWorker, worker.get()) != 0) {
            LOGE("Failed to create pool worker thread");
            pthread_attr_destroy(&attr);
            shutdownThreadPool(false);
            return false;
        }

        m_poolWorkers.push_back(std::move(worker));
        pthread_attr_destroy(&attr);
    }

    LOGI("Thread pool initialized with %zu workers", m_poolConfig.minThreads);
    return true;
}

bool AndroidThreadManager::submitTask(const std::string &taskName, ThreadTask task,
                                      ThreadPriority priority) {
    std::unique_lock<std::mutex> lock(m_poolMutex);

    if (m_poolShutdown) {
        LOGE("Thread pool is shutdown");
        return false;
    }

    // 检查队列大小限制
    if (m_poolConfig.queueSize > 0 && m_taskQueue.size() >= m_poolConfig.queueSize) {
        LOGE("Task queue is full");
        return false;
    }

    m_taskQueue.emplace(taskName, task, priority);
    m_poolCondition.notify_one();

    // 如果队列中有任务且工作线程数量小于最大值，创建新线程
    if (m_taskQueue.size() > 0 && m_poolWorkers.size() < m_poolConfig.maxThreads &&
        m_activeWorkers == m_poolWorkers.size()) {
        auto worker = std::make_unique<PoolWorker>(this);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if (pthread_create(&worker->thread, &attr, threadPoolWorker, worker.get()) == 0) {
            m_poolWorkers.push_back(std::move(worker));
            LOGD("Added new worker thread, total: %zu", m_poolWorkers.size());
        }

        pthread_attr_destroy(&attr);
    }

    return true;
}

bool AndroidThreadManager::cancelTask(const std::string &taskName) {
    std::unique_lock<std::mutex> lock(m_poolMutex);

    // 遍历队列查找并移除任务
    std::queue<PoolTask> newQueue;
    bool found = false;

    while (!m_taskQueue.empty()) {
        auto task = m_taskQueue.front();
        m_taskQueue.pop();

        if (task.name == taskName) {
            found = true;
            LOGI("Task %s cancelled", taskName.c_str());
        } else {
            newQueue.push(task);
        }
    }

    m_taskQueue = std::move(newQueue);
    return found;
}

void AndroidThreadManager::shutdownThreadPool(bool waitForCompletion) {
    std::unique_lock<std::mutex> lock(m_poolMutex);

    if (m_poolShutdown) {
        return;
    }

    m_poolShutdown = true;
    m_poolCondition.notify_all();

    if (waitForCompletion) {
        // 等待所有任务完成
        while (!m_taskQueue.empty()) {
            lock.unlock();
            usleep(100000); // 100ms
            lock.lock();
        }
    } else {
        // 清空任务队列
        while (!m_taskQueue.empty()) {
            m_taskQueue.pop();
        }
    }

    // 通知所有工作线程停止
    for (auto &worker: m_poolWorkers) {
        worker->shouldStop = true;
    }

    m_poolCondition.notify_all();
    lock.unlock();

    // 等待所有工作线程退出
    for (auto &worker: m_poolWorkers) {
        pthread_join(worker->thread, nullptr);
    }

    m_poolWorkers.clear();
    m_activeWorkers = 0;

    LOGI("Thread pool shutdown completed");
}

std::unordered_map<std::string, ThreadState> AndroidThreadManager::getAllThreadStates() {
    std::unique_lock<std::mutex> lock(m_threadsMutex);
    std::unordered_map<std::string, ThreadState> states;

    for (const auto &pair: m_threads) {
        std::unique_lock<std::mutex> threadLock(pair.second->mutex);
        states[pair.first] = pair.second->state;
    }

    return states;
}

void AndroidThreadManager::cleanupCompletedThreads() {
    std::unique_lock<std::mutex> lock(m_threadsMutex);
    std::vector<std::string> toRemove;

    for (const auto &pair: m_threads) {
        std::unique_lock<std::mutex> threadLock(pair.second->mutex);
        if (pair.second->state == THREAD_STATE_COMPLETED ||
            pair.second->state == THREAD_STATE_STOPPED ||
            pair.second->state == THREAD_STATE_ERROR) {
            toRemove.push_back(pair.first);
        }
    }

    for (const auto &name: toRemove) {
        auto data = m_threads[name];
        pthread_join(data->thread, nullptr);
        m_threads.erase(name);
        LOGI("Cleaned up thread: %s", name.c_str());
    }
}

size_t AndroidThreadManager::getActiveThreadCount() {
    std::unique_lock<std::mutex> lock(m_threadsMutex);
    size_t count = 0;

    for (const auto &pair: m_threads) {
        std::unique_lock<std::mutex> threadLock(pair.second->mutex);
        if (pair.second->state == THREAD_STATE_RUNNING) {
            count++;
        }
    }

    return count;
}

size_t AndroidThreadManager::getPendingTaskCount() {
    std::unique_lock<std::mutex> lock(m_poolMutex);
    return m_taskQueue.size();
}

bool AndroidThreadManager::setCurrentThreadName(const std::string &name) {
    return (pthread_setname_np(pthread_self(), name.c_str()) == 0);
}

bool AndroidThreadManager::setCurrentThreadPriority(ThreadPriority priority) {
    // 转换为系统优先级值
    int sysPriority;
    switch (priority) {
        case PRIORITY_IDLE:
            sysPriority = 1;
            break;
        case PRIORITY_LOW:
            sysPriority = 10;
            break;
        case PRIORITY_BELOW_NORMAL:
            sysPriority = 15;
            break;
        case PRIORITY_NORMAL:
            sysPriority = 20;
            break;
        case PRIORITY_ABOVE_NORMAL:
            sysPriority = 25;
            break;
        case PRIORITY_HIGH:
            sysPriority = 30;
            break;
        case PRIORITY_REALTIME:
            sysPriority = 40;
            break;
        default:
            sysPriority = 20;
    }

    // 设置优先级
    setpriority(PRIO_PROCESS, 0, sysPriority);
    return true;
}

int AndroidThreadManager::getCurrentThreadId() {
    return static_cast<int>(syscall(SYS_gettid));
}

void *AndroidThreadManager::threadFunction(void *arg) {
    ThreadData *data = static_cast<ThreadData *>(arg);

    // 设置线程名称
    pthread_setname_np(pthread_self(), data->name.c_str());

    // 设置线程优先级
    setCurrentThreadPriority(data->priority);

    // 存储线程ID
    data->tid = getCurrentThreadId();

    // 附加到JVM（如果需要）
    JNIEnv *env = nullptr;
    bool attached = false;
    if (data->jvm) {
        env = attachJVM(data->jvm, data->name);
        if (env) {
            attached = true;
        }
    }

    {
        std::unique_lock<std::mutex> lock(data->mutex);
        data->state = THREAD_STATE_RUNNING;
    }

    // 执行任务
    try {
        // 检查是否应该停止
        {
            std::unique_lock<std::mutex> lock(data->mutex);
            if (data->shouldStop) {
                data->state = THREAD_STATE_STOPPED;
                if (attached) {
                    detachJVM(data->jvm);
                }
                return nullptr;
            }
        }

        // 执行任务
        if (data->task) {
            data->task();
        }

        // 标记为完成
        {
            std::unique_lock<std::mutex> lock(data->mutex);
            data->state = THREAD_STATE_COMPLETED;
        }
    } catch (const std::exception &e) {
        LOGE("Exception in thread %s: %s", data->name.c_str(), e.what());
        std::unique_lock<std::mutex> lock(data->mutex);
        data->state = THREAD_STATE_ERROR;
    } catch (...) {
        LOGE("Unknown exception in thread %s", data->name.c_str());
        std::unique_lock<std::mutex> lock(data->mutex);
        data->state = THREAD_STATE_ERROR;
    }

    // 从JVM分离
    if (attached) {
        detachJVM(data->jvm);
    }

    return nullptr;
}

void *AndroidThreadManager::threadPoolWorker(void *arg) {
    PoolWorker *worker = static_cast<PoolWorker *>(arg);
    AndroidThreadManager *manager = worker->manager;

    // 设置线程名称
    std::string threadName = "PoolWorker-" + std::to_string(getCurrentThreadId());
    pthread_setname_np(pthread_self(), threadName.c_str());

    // 存储线程ID
    worker->tid = getCurrentThreadId();

    // 附加到JVM
    JNIEnv *env = attachJVM(manager->m_javaVM, threadName);
    bool attached = (env != nullptr);

    while (!worker->shouldStop) {
        PoolTask task("", nullptr, PRIORITY_NORMAL);
        bool hasTask = false;

        {
            std::unique_lock<std::mutex> lock(manager->m_poolMutex);
            manager->m_activeWorkers--;

            // 等待任务或超时
            if (manager->m_taskQueue.empty()) {
                // 使用超时等待，避免线程永久阻塞
                auto timeout = std::chrono::milliseconds(manager->m_poolConfig.idleTimeoutMs);
                auto status = manager->m_poolCondition.wait_for(lock, timeout);

                // 如果超时且线程数超过最小值，退出线程
                if (status == std::cv_status::timeout &&
                    manager->m_poolWorkers.size() > manager->m_poolConfig.minThreads) {
                    break;
                }
            }

            // 获取任务
            if (!manager->m_taskQueue.empty()) {
                task = manager->m_taskQueue.front();
                manager->m_taskQueue.pop();
                hasTask = true;
                manager->m_activeWorkers++;
            }

            if (worker->shouldStop) {
                break;
            }
        }

        // 执行任务
        if (hasTask && task.task) {
            try {
                // 设置任务优先级
                setCurrentThreadPriority(task.priority);

                // 执行任务
                task.task();

                LOGD("Task %s completed", task.name.c_str());
            } catch (const std::exception &e) {
                LOGE("Exception in task %s: %s", task.name.c_str(), e.what());
            } catch (...) {
                LOGE("Unknown exception in task %s", task.name.c_str());
            }
        }
    }

    // 从JVM分离
    if (attached) {
        detachJVM(manager->m_javaVM);
    }

    // 从工作线程列表中移除
    {
        std::unique_lock<std::mutex> lock(manager->m_poolMutex);
        for (auto it = manager->m_poolWorkers.begin(); it != manager->m_poolWorkers.end(); ++it) {
            if ((*it)->tid == worker->tid) {
                manager->m_poolWorkers.erase(it);
                break;
            }
        }
    }

    LOGD("Pool worker thread exited");
    return nullptr;
}

JNIEnv *AndroidThreadManager::attachJVM(JavaVM *jvm, const std::string &threadName) {
    if (!jvm) {
        return nullptr;
    }

    JNIEnv *env = nullptr;
    jint result = jvm->GetEnv(reinterpret_cast<void **>(&env), JNI_VERSION_1_6);

    if (result == JNI_EDETACHED) {
        JavaVMAttachArgs args;
        args.version = JNI_VERSION_1_6;
        args.name = threadName.c_str();
        args.group = nullptr;

        if (jvm->AttachCurrentThread(&env, &args) == JNI_OK) {
            LOGD("Thread %s attached to JVM", threadName.c_str());
            return env;
        }
    } else if (result == JNI_OK) {
        return env;
    }

    LOGE("Failed to attach thread %s to JVM", threadName.c_str());
    return nullptr;
}

void AndroidThreadManager::detachJVM(JavaVM *jvm) {
    if (jvm) {
        jvm->DetachCurrentThread();
        LOGD("Thread detached from JVM");
    }
}