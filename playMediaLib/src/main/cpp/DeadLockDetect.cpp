//  Author : wangyongyao https://github.com/wangyongyao1989
// Created by MMM on 2025/12/9.
// Dec :死锁检查机制
//
#define _GNU_SOURCE

#include <dlfcn.h>
#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#define MAX 100

// 节点：仅描述线程（等待图的核心是线程依赖）
typedef struct _node {
    uintptr_t threadid;
} node;

typedef struct _vertex {
    node n;
    struct _vertex *next;
} vertex;

// 锁持有记录：锁ID -> 持有者线程ID + 等待数
typedef struct _lock_entry {
    uintptr_t lockid;
    uintptr_t holder; // 持有者线程ID
    int wait_cnt;     // 等待该锁的线程数（关键：解决多等待者场景）
} lock_entry;

typedef struct _thread_graph {
    vertex list[MAX];         // 邻接表：线程 -> 等待的线程
    int num;                  // 当前有效线程节点数
    lock_entry locklist[MAX]; // 锁持有表
    int lock_num;             // 有效锁记录数（优化遍历效率）
    pthread_mutex_t graph_mtx;
} thread_graph;

thread_graph *tg = nullptr;      // 死锁检测全局状态（加锁保护）
static char color[MAX];       // 0=WHITE, 1=GRAY, 2=BLACK
static int parent[MAX];       // 路径回溯
static int deadlock = 0;
static pthread_mutex_t detect_print_mtx = PTHREAD_MUTEX_INITIALIZER;

// ==================== 图操作（核心）====================
vertex *create_vertex(node n) {
    vertex *v = static_cast<vertex *>(calloc(1, sizeof(vertex)));
    if (!v) {
        perror("calloc vertex failed");
        exit(EXIT_FAILURE);
    }
    v->n = n;
    return v;
}

// 查找线程对应的顶点索引（核心优化：仅遍历有效节点）
int search_vertex(uintptr_t tid) {
    for (int i = 0; i < tg->num; i++) {
        if (tg->list[i].n.threadid == tid) {
            return i;
        }
    }
    return -1;
}

// 添加线程顶点（防越界）
void add_vertex(uintptr_t tid) {
    if (search_vertex(tid) != -1)
        return; // 已存在
    if (tg->num >= MAX) {
        fprintf(stderr, "Error: thread num exceed MAX(%d)\n", MAX);
        return;
    }
    tg->list[tg->num].n.threadid = tid;
    tg->list[tg->num].next = nullptr;
    tg->num++;
}

// 添加等待边：from_tid 等待 to_tid 持有的锁
void add_edge(uintptr_t from_tid, uintptr_t to_tid) {
    if (from_tid == to_tid)
        return; // 跳过自环
    add_vertex(from_tid);
    add_vertex(to_tid);
    int from_idx = search_vertex(from_tid);
    if (from_idx == -1)
        return;    // 避免重复边（核心：防止图膨胀）
    vertex *cur = &tg->list[from_idx];
    while (cur->next) {
        if (cur->next->n.threadid == to_tid) return;
        cur = cur->next;
    }
    cur->next = create_vertex((node) {.threadid = to_tid});
}

// 移除等待边（释放内存，避免泄漏）
int remove_edge(uintptr_t from_tid, uintptr_t to_tid) {
    int from_idx = search_vertex(from_tid);
    if (from_idx == -1) return -1;
    vertex *cur = &tg->list[from_idx];
    while (cur->next) {
        if (cur->next->n.threadid == to_tid) {
            vertex *tmp = cur->next;
            cur->next = tmp->next;
            free(tmp);
            return 0;
        }
        cur = cur->next;
    }
    return -1;
}

// ==================== 锁状态管理（核心修正）====================
// 查找锁记录（仅遍历有效锁）
int find_lock_entry(uintptr_t lockid) {
    for (int i = 0; i < tg->lock_num; i++) {
        if (tg->locklist[i].lockid == lockid) {
            return i;
        }
    }
    return -1;
}

// 查找空的锁槽位
int find_empty_lock_slot() {
    for (int i = 0; i < MAX; i++) {
        if (tg->locklist[i].lockid == 0) {
            return i;
        }
    }
    return -1;
}

// ==================== 死锁检测（三色法 + 精准路径打印）====================
static void print_cycle_path(int start, int end) {
    int path[MAX];
    int len = 0;
    int cur = end;    // 从end回溯到start，构建环路径
    while (cur != -1 && len < MAX) {
        path[len++] = cur;
        if (cur == start) break;
        cur = parent[cur];
    }
    if (cur != start || len < 2)
        return; // 未形成有效环
    // 加锁避免重复打印
    pthread_mutex_lock(&detect_print_mtx);
    if (deadlock) {
        pthread_mutex_unlock(&detect_print_mtx);
        return;
    }
    deadlock = 1;

    // 打印环（修正重复拼接问题）
    printf("\n===== DEADLOCK DETECTED =====\n");
    printf("Cycle path: ");
    for (int i = len - 1; i >= 0; i--) {
        printf("%lx", (unsigned long) tg->list[path[i]].n.threadid);
        if (i > 0)
            printf(" --> ");
    }
    printf("\n=============================\n");
    pthread_mutex_unlock(&detect_print_mtx);
}

// 深度优先检测环（三色法核心）
static int dfs_detect(int u) {
    color[u] = 1; // 标记为GRAY（递归栈中）
    vertex *vtx = &tg->list[u];
    while (vtx->next) {
        int v_idx = search_vertex(vtx->next->n.threadid);
        if (v_idx == -1) {
            vtx = vtx->next;
            continue;
        }

        if (color[v_idx] == 0) { // WHITE：未访问
            parent[v_idx] = u;
            if (dfs_detect(v_idx))
                return 1; // 递归检测        }
            else if (color[v_idx] == 1) { // GRAY：检测到环
                print_cycle_path(v_idx, u);
                return 1;
            }
            vtx = vtx->next;
        }
        color[u] = 2; // 标记为BLACK（已处理完成）
        return 0;
    }
}

// 死锁检测主函数
static void check_deadlock_internal() {
    if (tg->num == 0) {
        printf("[Detect] No threads to check\n");
        return;
    }
    // 重置检测状态
    memset(color, 0, sizeof(color));
    memset(parent, -1, sizeof(parent));
    deadlock = 0;

    // 遍历所有未访问节点
    for (int i = 0; i < tg->num && !deadlock; i++) {
        if (color[i] == 0) {
            if (dfs_detect(i))
                break; // 检测到死锁，终止遍历
        }
    }

    if (!deadlock) {
        printf("[Detect] No deadlock detected\n");
    }
}

// ==================== Hook 函数（核心修正）====================
typedef int (*real_pthread_mutex_lock_t)(pthread_mutex_t *);

typedef int (*real_pthread_mutex_unlock_t)(pthread_mutex_t *);

static real_pthread_mutex_lock_t real_pthread_mutex_lock = nullptr;
static real_pthread_mutex_unlock_t real_pthread_mutex_unlock = nullptr;

// 加锁前：构建等待边 + 增加锁等待数
void lock_before(uintptr_t self, uintptr_t lock) {
    pthread_mutex_lock(&tg->graph_mtx);
    int idx = find_lock_entry(lock);
    if (idx != -1) {
        uintptr_t holder = tg->locklist[idx].holder;
        if (holder != 0 && holder != self) {
            add_edge(self, holder); // 构建：self → holder
            tg->locklist[idx].wait_cnt++; // 等待数+1
        }
    }
    pthread_mutex_unlock(&tg->graph_mtx);
}


// 加锁后：更新锁持有者 + 移除等待边
void lock_after(uintptr_t self, uintptr_t lock) {
    pthread_mutex_lock(&tg->graph_mtx);
    int idx = find_lock_entry(lock);
    if (idx == -1) {        // 新增锁记录
        int slot = find_empty_lock_slot();
        if (slot == -1) {
            fprintf(stderr, "Error: lock table full\n");
        } else {
            tg->locklist[slot].lockid = lock;
            tg->locklist[slot].holder = self;
            tg->locklist[slot].wait_cnt = 0;
            tg->lock_num++;
        }
    } else {
        // 移除旧的等待边
        uintptr_t old_holder = tg->locklist[idx].holder;
        if (old_holder != self) {
            remove_edge(self, old_holder);
            tg->locklist[idx].wait_cnt--; // 等待数-1
        }
        // 更新持有者
        tg->locklist[idx].holder = self;
    }
    pthread_mutex_unlock(&tg->graph_mtx);
}


// 解锁后：仅当无等待者时清空锁记录（核心修正）
void unlock_after(uintptr_t self, uintptr_t lock) {
    pthread_mutex_lock(&tg->graph_mtx);
    int idx = find_lock_entry(lock);
    if (idx != -1 && tg->locklist[idx].holder == self) {
        // 只有无等待者时才清空，否则保留锁记录（供后续等待者使用）
        if (tg->locklist[idx].wait_cnt == 0) {
            tg->locklist[idx].lockid = 0;
            tg->locklist[idx].holder = 0;
            tg->lock_num--; // 有效锁数-1
        }
    }
    pthread_mutex_unlock(&tg->graph_mtx);
}

// Hook pthread_mutex_lock
int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_lock) {
        fprintf(stderr, "Error: real_pthread_mutex_lock not init\n");
        return EINVAL;
    }
    uintptr_t self = (uintptr_t) pthread_self();
    uintptr_t lock = (uintptr_t) mutex;
    lock_before(self, lock);
    int ret = real_pthread_mutex_lock(mutex); // 可能阻塞
    lock_after(self, lock);
    return ret;
}

// Hook pthread_mutex_unlock
int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (!real_pthread_mutex_unlock) {
        fprintf(stderr, "Error: real_pthread_mutex_unlock not init\n");
        return EINVAL;
    }
    uintptr_t self = (uintptr_t) pthread_self();
    uintptr_t lock = (uintptr_t) mutex;
    int ret = real_pthread_mutex_unlock(mutex);
    unlock_after(self, lock);
    return ret;
}


// ==================== 初始化与检测线程 ====================
static int init_hooks() {
    // 规范的dlsym错误处理：先清空错误
    dlerror();
    real_pthread_mutex_lock = (real_pthread_mutex_lock_t) dlsym(RTLD_NEXT,
                                                                "pthread_mutex_lock");
    const char *err = dlerror();
    if (err != nullptr) {
        fprintf(stderr, "dlsym pthread_mutex_lock failed: %s\n", err);
        return -1;
    }
    dlerror();
    real_pthread_mutex_unlock = (real_pthread_mutex_unlock_t) dlsym(RTLD_NEXT,
                                                                    "pthread_mutex_unlock");
    err = dlerror();
    if (err != nullptr) {
        fprintf(stderr, "dlsym pthread_mutex_unlock failed: %s\n", err);
        return -1;
    }
    return 0;
}

// 死锁检测线程（低频率打印，减少冗余）
static void *detect_thread_func(void *arg) {
    int detect_cnt = 0;
    while (1) {
        sleep(3); // 延长检测间隔，减少日志干扰
        pthread_mutex_lock(&tg->graph_mtx);
        if (detect_cnt % 2 == 0) { // 每2次检测打印一次提示
            printf("\n[Detect] Round %d: Scanning deadlocks...\n", detect_cnt + 1);
        }
        check_deadlock_internal();
        pthread_mutex_unlock(&tg->graph_mtx);
        detect_cnt++;
    }
    return nullptr;
}


// 启动死锁检测器
static void start_detector() {
    // 初始化图结构
    tg = static_cast<thread_graph *>(calloc(1, sizeof(thread_graph)));
    if (!tg) {
        perror("calloc thread_graph failed");
        exit(EXIT_FAILURE);
    }
    pthread_mutex_init(&tg->graph_mtx, nullptr);
    memset(tg->locklist, 0, sizeof(tg->locklist));
    tg->lock_num = 0;
    // 创建检测线程
    pthread_t tid;
    if (pthread_create(&tid, nullptr, detect_thread_func, nullptr)) {
        perror("pthread_create detect thread failed");
        exit(EXIT_FAILURE);
    }
    pthread_detach(tid); // 分离线程，避免资源泄漏
    printf("Deadlock detector initialized successfully.\n");
}

// ==================== 测试用例（确保死锁触发）====================
pthread_mutex_t mtx1 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx2 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx3 = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtx4 = PTHREAD_MUTEX_INITIALIZER;

// 线程A：mtx1 → mtx2
void *thread_a(void *_) {
    printf("Thread A (tid: %lx) started\n", (unsigned long) pthread_self());
    pthread_mutex_lock(&mtx1);
    sleep(4); // 延长sleep，确保环形等待形成
    pthread_mutex_lock(&mtx2); // 等待线程B释放mtx2
    // 死锁时无法执行到此处
    pthread_mutex_unlock(&mtx2);
    pthread_mutex_unlock(&mtx1);
    printf("Thread A ended\n");
    return nullptr;
}

// 线程B：mtx2 → mtx3
void *thread_b(void *_) {
    printf("Thread B (tid: %lx) started\n", (unsigned long) pthread_self());
    pthread_mutex_lock(&mtx2);
    sleep(4);
    pthread_mutex_lock(
            &mtx3); // 等待线程C释放mtx3
    pthread_mutex_unlock(&mtx3);
    pthread_mutex_unlock(&mtx2);
    printf("Thread B ended\n");
    return nullptr;
}

// 线程C：mtx3 → mtx4
void *thread_c(void *_) {
    printf("Thread C (tid: %lx) started\n", (unsigned long) pthread_self());
    pthread_mutex_lock(&mtx3);
    sleep(4);
    pthread_mutex_lock(
            &mtx4); // 等待线程D释放mtx4
    pthread_mutex_unlock(&mtx4);
    pthread_mutex_unlock(&mtx3);
    printf("Thread C ended\n");
    return nullptr;
}

// 线程D：mtx4 → mtx1（形成环：A→B→C→D→A）
void *thread_d(void *_) {
    printf("Thread D (tid: %lx) started\n", (unsigned long) pthread_self());
    pthread_mutex_lock(&mtx4);
    sleep(4);
    pthread_mutex_lock(
            &mtx1); // 等待线程A释放mtx1
    pthread_mutex_unlock(&mtx1);
    pthread_mutex_unlock(&mtx4);
    printf("Thread D ended\n");
    return nullptr;
}

// ==================== 主函数 ====================
int main() {
    // 初始化Hook
    if (init_hooks() != 0) {
        fprintf(stderr, "Failed to init hooks\n");
        return EXIT_FAILURE;
    }

    // 启动死锁检测器
    start_detector();
    // 创建测试线程
    pthread_t t1, t2, t3, t4;
    pthread_create(&t1, nullptr, thread_a, nullptr);
    pthread_create(&t2, nullptr, thread_b, nullptr);
    pthread_create(&t3, nullptr, thread_c, nullptr);
    pthread_create(&t4, nullptr, thread_d, nullptr);
    // 等待线程（死锁时会阻塞）
    pthread_join(t1, nullptr);
    pthread_join(t2, nullptr);
    pthread_join(t3, nullptr);
    pthread_join(t4, nullptr);
    // 清理资源（死锁时不会执行）
    pthread_mutex_destroy(&tg->graph_mtx);
    free(tg);
    return EXIT_SUCCESS;
}
