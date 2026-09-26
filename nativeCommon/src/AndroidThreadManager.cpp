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
    std::vector<pthread_t> joinableThreads;
    {
        std::unique_lock<std::mutex> lock(m_threadsMutex);
        for (auto &pair: m_threads) {
            auto &data = pair.second;

            {
                std::unique_lock<std::mutex> threadLock(data->mutex);
                // 必须先取改写前的状态：原来这里先把 state 置成 STOPPING，再在锁外判断
                // state == RUNNING || PAUSED，条件永远不成立，一个线程都不会被 join，
                // 管理器析构后工作线程仍在访问已释放的 ThreadData。
                data->shouldStop = true;
                data->state = THREAD_STATE_STOPPING;
                data->condition.notify_all();
            }

            // pthread_create 成功后才会进 m_threads，所以这里的 thread 一定是 joinable 的；
            // 已结束的线程 join 会立即返回，同时回收线程资源。
            joinableThreads.push_back(data->thread);
        }
        m_threads.clear();
    }

    // join 放在 m_threadsMutex 之外，避免被 join 的线程回调里再取同一把锁时互等
    for (auto &thread: joinableThreads) {
        pthread_join(thread, nullptr);
    }
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

    // 先回收已超时退出的线程，否则 m_poolWorkers 里残留的 exiting 项会让下面的
    // "already initialized" 判断永远成立（hwCodecLib 每次点按钮都会再调一次 initThreadPool）。
    reapExitedWorkers();

    if (!m_poolWorkers.empty()) {
        LOGE("Thread pool already initialized");
        return false;
    }

    m_poolConfig = config;
    m_poolShutdown = false;
    m_activeWorkers = 0;

    // 创建最小数量的工作线程
    for (size_t i = 0; i < m_poolConfig.minThreads; ++i) {
        auto worker = std::make_unique<PoolWorker>(this);

        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);

        if (pthread_create(&worker->thread, &attr, threadPoolWorker, worker.get()) != 0) {
            LOGE("Failed to create pool worker thread");
            pthread_attr_destroy(&attr);
            // 这里已经持有 m_poolMutex，必须走持锁版本：
            // 直接调 shutdownThreadPool 会在同一把非递归锁上二次加锁，卡死。
            shutdownLocked(lock, false);
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

    // 先回收超时退出的工作线程，再判断是否需要扩容
    reapExitedWorkers();

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

void AndroidThreadManager::reapExitedWorkers() {
    // 调用方必须持有 m_poolMutex。
    // 只清理工作线程自己置好 exiting 标记的那一项：这些线程置标记后只剩 detachJVM + return，
    // 不会再回来抢 m_poolMutex，所以在持锁状态下 join 不会互等。
    bool removed = true;
    while (removed) {
        removed = false;
        for (auto it = m_poolWorkers.begin(); it != m_poolWorkers.end(); ++it) {
            if ((*it)->exiting.load()) {
                pthread_join((*it)->thread, nullptr);
                m_poolWorkers.erase(it);
                removed = true;
                break;
            }
        }
    }
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
    shutdownLocked(lock, waitForCompletion);
}

// 调用方必须持有 m_poolMutex（用 unique_lock 引用传入，内部会临时解锁 join）。
// 单独拆出来是因为 initThreadPool 的 pthread_create 失败分支本来就持着 m_poolMutex，
// 直接调 shutdownThreadPool 会对同一把非递归锁二次加锁 —— 死锁。
void AndroidThreadManager::shutdownLocked(std::unique_lock<std::mutex> &lock,
                                          bool waitForCompletion) {
    if (m_poolShutdown) {
        return;
    }

    m_poolShutdown = true;
    m_poolCondition.notify_all();

    if (waitForCompletion) {
        // 等待队列排空且没有任务在执行，用条件变量替代原来的 unlock + usleep(100ms) 轮询。
        // 设上限兜底：工作线程可能已经全部超时退出，此时不应无限等下去。
        const auto deadline = std::chrono::steady_clock::now()
                              + std::chrono::milliseconds(m_poolConfig.idleTimeoutMs + 5000);
        while ((!m_taskQueue.empty() || m_activeWorkers.load() != 0)
               && m_poolCondition.wait_until(lock, deadline) != std::cv_status::timeout) {
        }
    } else {
        // 清空任务队列
        while (!m_taskQueue.empty()) {
            m_taskQueue.pop();
        }
    }

    // 通知所有工作线程停止，并先把句柄抄出来：
    // join 期间不持锁，也不依赖 m_poolWorkers 元素的稳定性（原来边遍历 vector 边 join，
    // 工作线程超时自删会使元素前移/析构，读到的 worker->thread 已是野值）。
    std::vector<pthread_t> threads;
    threads.reserve(m_poolWorkers.size());
    for (auto &worker: m_poolWorkers) {
        worker->shouldStop = true;
        threads.push_back(worker->thread);
    }

    m_poolCondition.notify_all();
    lock.unlock();

    // 等待所有工作线程退出
    for (auto &thread: threads) {
        pthread_join(thread, nullptr);
    }

    lock.lock();
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

            if (worker->shouldStop) {
                break;
            }

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
                // m_activeWorkers 的口径是"正在执行任务的工作线程数"，取到任务时 +1、
                // 执行完 -1。原来每轮先无条件 --、只有取到任务才 ++，空闲一轮就净 -1，
                // size_t 直接下溢成大数，submitTask 里 m_activeWorkers == m_poolWorkers.size()
                // 的扩容条件永远不成立，maxThreads 配置形同废弃。
                manager->m_activeWorkers++;
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

            std::unique_lock<std::mutex> lock(manager->m_poolMutex);
            manager->m_activeWorkers--;
            // 唤醒 shutdownThreadPool(true) 的"队列已空且无任务在执行"等待
            manager->m_poolCondition.notify_all();
        }
    }

    // 从JVM分离
    if (attached) {
        detachJVM(manager->m_javaVM);
    }

    // 不在工作线程里把自己从 m_poolWorkers 中 erase：
    // m_poolWorkers 是 vector<unique_ptr<PoolWorker>>，自己 erase 会当场析构自己的 PoolWorker，
    // 并且后面的元素前移，正在遍历该 vector 的一方（shutdown）拿到的就是野指针。
    // 这里只打标记，join + erase 由持有 m_poolMutex 的 reapExitedWorkers() 完成。
    {
        std::unique_lock<std::mutex> lock(manager->m_poolMutex);
        worker->exiting.store(true);
        manager->m_poolCondition.notify_all();
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