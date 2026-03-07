# 轻量级分级中断VM调度框架（LIVS）项目开发手册 & 开发进度规划


目录结构：  
```Text
LIVS/
 ├── CMakeLists.txt
 ├── include/
 │    ├── vm/
 │    ├── scheduler/
 │    ├── interrupt/
 │    ├── periph/
 │    └── utils/
 ├── src/
 │    ├── vm/
 │    ├── scheduler/
 │    ├── interrupt/
 │    ├── periph/
 │    └── utils/
 ├── tests/
 ├── demo/
 └── docs/
```

错误码体系：
```C++
enum class ErrorCode {
    OK = 0,
    VM_NOT_FOUND,
    GIL_TIMEOUT,
    QUEUE_FULL,
    VM_DESTROYED,
    RESOURCE_LIMIT_EXCEEDED
};
```

当前版本：一个VM = 一个宿主线程（可绑定CPU亲和性）


## 一、项目概述

### 1.1 项目定位

本项目是**基于宿主OS的附加型轻量级VM调度框架**（练手级系统开发项目），完整落地你之前定义的核心设计：

- 核心准则：**需要返回值=应急同步中断（VM挂起等待）、无返回值=可磨蹭异步中断（中断等待VM）**

- 配套能力：外设级GIL、叫号模型、VM资源隔离、有界队列防堆爆炸、非空转调度

- 适用场景：系统开发/内核原理练手、轻量级虚拟化调度、外设中断差异化管理

- 非目标：不做完整独立内核、不做工业级生产环境部署，优先保证核心原理落地与练手价值

### 1.2 核心设计纲领（不可突破）

1. **中断分级准则**：同步中断（需返回值）保证响应性，异步中断（无返回值）保证CPU利用率，禁止异步中断阻塞VM主线程

2. **内存安全准则**：有界队列+对象池内存复用，绝对杜绝无界排队导致的堆爆炸

3. **资源隔离准则**：每个VM拥有独立上下文、独立堆空间、资源限额，单个VM故障不影响全局

4. **非空转准则**：仅同步中断可挂起VM，异步中断全程后台执行，CPU始终执行有效任务

5. **容错兜底准则**：所有同步操作必须带超时，异常场景必须有资源释放逻辑，禁止死锁/内存泄漏

### 1.3 整体架构分层

```Text

┌─────────────────────────────────────────────────┐
│  VM层                                           │
│  ├─ VM独立上下文（资源限额、运行状态、生命周期）│
│  └─ VM独立堆空间（与内核模拟堆隔离）           │
├─────────────────────────────────────────────────┤
│  中断调度核心层                                 │
│  ├─ 分级中断处理（同步/异步差异化调度）        │
│  ├─ 叫号模型（等待队列、超时清理、唤醒机制）   │
│  └─ VM状态管理（inList挂起标记、结果表）       │
├─────────────────────────────────────────────────┤
│  外设抽象层                                     │
│  ├─ 外设级GIL（多通道、互斥锁、超时释放）     │
│  ├─ 有界等待队列（防堆爆炸、限流）             │
│  └─ 外设操作抽象（兼容不同外设类型）           │
├─────────────────────────────────────────────────┤
│  宿主OS适配层                                   │
│  ├─ 线程封装（模拟CPU核、CPU亲和性绑定）       │
│  ├─ 同步原语封装（互斥锁、条件变量、原子变量） │
│  └─ 内存管理封装（对象池、堆空间隔离）         │
└─────────────────────────────────────────────────┘
```
禁止不同区域之间相互引用，如需传输数据必须通过路由器转发：
1. 禁止跨层调用
2. 禁止VM层直接访问Periph层
3. 禁止调度器直接修改VM堆
否则后期极易出现“跨层耦合污染”。

### 1.4 开发&运行环境

|类别|要求（练手友好型）|
|---|---|
|宿主OS|Ubuntu 20.04+ / CentOS 8+（Linux优先，原生支持pthread、CPU亲和性）|
|编译器|GCC 7.5+ / Clang 10+（必须支持C++17）|
|构建工具|CMake 3.10+|
|调试工具|gdb（代码调试）、valgrind（内存泄漏/死锁检测）|
|可选工具|QEMU（后续裸机移植）、VS Code + C++插件（开发）|
---

## 二、核心模块开发手册

### 2.1 基础工具模块（项目基石）

#### 模块职责

提供内存复用、队列限流、日志打印等基础能力，是所有模块的依赖，杜绝后续开发中的内存泄漏、无界排队问题。

#### 核心子模块&接口

1. **对象池模板类 ** **`ObjectPool<T>`**

    - 核心接口：

        ```C++
        
        template <typename T>
        class ObjectPool {
        public:
            explicit ObjectPool(int max_size); // 池最大容量（防堆爆炸）
            std::unique_ptr<T> acquire();      // 获取对象（复用空闲/新建）
            void release(std::unique_ptr<T> obj); // 归还对象（复用）
        private:
            std::list<std::unique_ptr<T>> free_pool;
            std::mutex pool_mtx;
            int max_capacity;
        };
        ```

    - 实现规范：池满禁止新建对象，直接返回nullptr；归还对象必须重置状态，避免脏数据。

2. **有界等待队列类 ** **`BoundedWaitQueue`**

    - 核心接口：

        ```C++
        
        struct WaitTask;
        class BoundedWaitQueue {
        public:
            explicit BoundedWaitQueue(int max_wait_num); // 最大等待数
            bool add_task(std::unique_ptr<WaitTask> task); // 添加任务（超上限拒绝）
            std::unique_ptr<WaitTask> get_task(uint64_t vm_num); // 取出指定VM的任务
            void clean_timeout_tasks(int timeout_sec); // 清理超时任务
        private:
            std::queue<std::unique_ptr<WaitTask>> task_queue;
            std::mutex queue_mtx;
            std::atomic<int> current_count;
            int max_capacity;
        };
        ```

    - 实现规范：所有队列操作必须加锁；超时任务必须释放内存/归还对象池；禁止队列无限增长。

3. **日志工具类 ** **`Logger`**

    - 核心接口：`DEBUG/INFO/WARN/ERROR` 四个级别日志，支持VM编号、模块名打印，方便调试。

### 2.2 VM上下文模块（资源隔离核心）

#### 模块职责

实现VM的资源隔离、生命周期管理，模拟真实VM的独立运行环境，杜绝VM间的内存污染、资源争抢。

#### 核心子模块&接口

1. **VM上下文结构体 ** **`VMContext`**

    ```C++
    
    struct VMContext {
        const uint64_t vm_id; // 唯一VM编号（uint64_t，对应inList索引）
        std::atomic<bool> is_running; // 运行状态
        // 资源限额
        const int cpu_limit; // 最大可用CPU核心数
        const size_t mem_limit_bytes; // 最大堆内存限额
        // 独立堆空间
        std::unordered_map<std::string, int> vm_heap;
        std::mutex heap_mtx;
        // VM内部锁
        std::mutex vm_mtx;
    
        explicit VMContext(uint64_t id, int cpu_lim, size_t mem_lim);
        bool check_resource_limit(); // 检查内存是否超限，超限拒绝分配
        void clear_resource(); // 释放VM所有资源（退出/崩溃兜底）
    };
    ```

2. **VM管理类 ** **`VMManager`**

    - 核心接口：`create_vm()` 创建VM、`destroy_vm()` 销毁VM、`get_vm()` 获取VM上下文、`list_vm()` 列出所有VM。

    - 实现规范：VM ID全局唯一；销毁VM必须强制释放所有资源，避免内存泄漏；VM间完全隔离，禁止跨VM直接访问。

### 2.3 VM状态管理模块（挂起/唤醒核心）

#### 模块职责

管理VM的挂起/运行状态，存储中断返回结果，对应你定义的`inList`全局数组。

#### 核心子模块&接口

1. **VM状态管理类 ** **`VMStateManager`**

    ```C++
    
    class VMStateManager {
    public:
        explicit VMStateManager(uint64_t max_vm_num);
        // 核心：inList操作
        void set_suspend(uint64_t vm_id, bool is_suspend); // 标记挂起/运行
        bool is_suspend(uint64_t vm_id); // 查询是否挂起
        // 中断结果表
        void set_interrupt_result(uint64_t vm_id, int result); // 存储返回值
        int get_interrupt_result(uint64_t vm_id); // 获取返回值
    private:
        std::vector<bool> in_list; // 核心：vm_id为索引，true=挂起
        std::mutex in_list_mtx;
        std::unordered_map<uint64_t, int> interrupt_result_table; // 中断结果表
        std::mutex result_mtx;
        uint64_t max_vm_count;
    };
    ```

    - 实现规范：所有in_list操作必须加锁，保证线程安全；VM销毁时必须清除对应状态和结果，避免脏数据。

### 2.4 外设抽象层（GIL+叫号模型核心）

#### 模块职责

实现外设级GIL、多通道负载均衡、叫号模型，解决外设争抢、空转、堆爆炸问题。

#### 核心子模块&接口

1. **外设GIL类 ** **`PeriphGIL`**

    ```C++
    
    class PeriphGIL {
    public:
        explicit PeriphGIL(int periph_id, int channel_id);
        bool try_lock(int timeout_ms); // 带超时尝试加锁
        void unlock(); // 释放锁
        int get_periph_id() const;
        int get_channel_id() const;
    private:
        const int periph_id;
        const int channel_id;
        std::timed_mutex gil_mutex; // 定时锁，支持超时
    };
    ```

2. **外设管理类 ** **`PeriphManager`**

    - 核心接口：

        ```C++
        
        class PeriphManager {
        public:
            explicit PeriphManager(int periph_num, int channel_per_periph);
            // 通道选择（vm_id哈希取模，均衡负载）
            int select_channel(uint64_t vm_id, int periph_id);
            // 获取对应通道的GIL
            PeriphGIL* get_gil(int periph_id, int channel_id);
            // 叫号模型：等待/唤醒
            void wait_for_periph(uint64_t vm_id, int periph_id, int timeout_ms);
            void wakeup_waiter(uint64_t vm_id, int periph_id);
        private:
            // 二维数组：[外设ID][通道ID] → GIL
            std::vector<std::vector<std::unique_ptr<PeriphGIL>>> gil_table;
            // 每个外设对应一个有界等待队列
            std::vector<std::unique_ptr<BoundedWaitQueue>> wait_queue_table;
            int periph_count;
            int channel_per_periph;
        };
        ```

    - 实现规范：每个外设独立多通道GIL，通道间完全并行；等待队列必须有界，禁止无限排队；叫号唤醒必须对应VM，避免虚假唤醒。

### 2.5 分级中断调度模块（项目核心灵魂）

#### 模块职责

落地你定义的中断分级准则，实现同步/异步中断的差异化调度，是整个项目的核心。

#### 核心子模块&接口

1. **中断类型枚举**

    ```C++
    
    enum class InterruptType {
        SYNC_WITH_RETURN,  // 应急中断：需要返回值，VM挂起等待
        ASYNC_NO_RETURN    // 可磨蹭中断：无返回值，异步后台执行
    };
    ```

2. **中断任务结构体 ** **`InterruptTask`**

    ```C++
    
    struct InterruptTask {
        uint64_t vm_id;
        InterruptType type;
        int periph_id;
        std::atomic<int> return_value; // 仅同步中断使用
        std::chrono::steady_clock::time_point create_time;
        bool is_timeout = false;
    };
    ```

3. **中断调度器类 ** **`InterruptScheduler`**

    - 核心接口：

        ```C++
        
        class InterruptScheduler {
        public:
            InterruptScheduler(VMStateManager* vm_state_mgr, PeriphManager* periph_mgr);
            // 统一中断入口（核心）
            int handle_interrupt(uint64_t vm_id, InterruptType type, int periph_id, int timeout_ms = 2000);
        private:
            // 同步中断处理（需返回值：VM挂起等待）
            int handle_sync_interrupt(uint64_t vm_id, int periph_id, int timeout_ms);
            // 异步中断处理（无返回值：后台执行，不阻塞VM）
            void handle_async_interrupt(uint64_t vm_id, int periph_id);
            
            VMStateManager* vm_state_manager;
            PeriphManager* periph_manager;
        };
        ```

    - 实现规范：

        - 同步中断必须带超时，超时强制释放GIL，避免永久挂起；

        - 同步中断必须修改`in_list`标记VM挂起，完成后唤醒VM；

        - 异步中断必须在独立后台线程执行，绝对禁止阻塞VM主线程；

        - 异步中断必须等待VM空闲后执行，不影响VM核心逻辑时序。

### 2.6 VM调度器模块

#### 模块职责

管理VM的运行/挂起队列，实现VM的上下文切换、唤醒/挂起，模拟内核调度器的核心逻辑。

#### 核心接口

```C++

class VMScheduler {
public:
    VMScheduler(VMManager* vm_mgr, VMStateManager* vm_state_mgr);
    // 队列操作
    void add_to_run_queue(uint64_t vm_id); // 加入运行队列
    void add_to_suspend_queue(uint64_t vm_id); // 加入挂起队列
    void wakeup_vm(uint64_t vm_id); // 从挂起队列唤醒到运行队列
    void suspend_vm(uint64_t vm_id); // 从运行队列挂起到挂起队列
    // 调度执行
    void run_scheduler(); // 启动调度循环
    void stop_scheduler(); // 停止调度
private:
    std::queue<uint64_t> run_queue;
    std::queue<uint64_t> suspend_queue;
    std::mutex sched_mtx;
    std::atomic<bool> is_running;
    VMManager* vm_manager;
    VMStateManager* vm_state_manager;
};
```

### ThreadPool模块
补充“后台线程执行”：
```C++
class ThreadPool {
public:
    submit(std::function<void()> task);
}
```
否则每个异步中断都开线程，后期会失控。

### TimeoutManager模块
统一管理超时，抽象为：
```C++
class TimeoutManager {
    register_timeout(task_id, duration);
}
```
统一处理超时扫描。

### 指标收集模块：
为6阶段做准备：
```C++
class MetricsCollector {
    record_interrupt_latency()
    record_gil_wait_time()
    record_queue_length()
}
```


---

## 三、开发规范（练手阶段必须遵守）

1. **代码规范**：遵循Google C++ 代码规范，类名大驼峰、函数名小写下划线、常量全大写，禁止拼音命名、无注释代码。

2. **内存管理规范**：

    - 禁止裸使用`new/delete`，所有动态内存必须用`std::unique_ptr/std::shared_ptr`管理；

    - 高频创建/销毁的对象必须使用对象池复用，避免频繁申请释放导致的堆碎片；

    - 严格遵守“谁申请、谁释放”，异常场景必须有资源释放兜底。

3. **线程安全规范**：

    - 所有跨线程共享的变量必须加互斥锁，或用`std::atomic`原子变量；

    - 禁止在锁内执行耗时操作、阻塞操作；

    - 条件变量必须配合`while`循环判断状态，禁止用`if`判断，避免虚假唤醒。

4. **中断处理规范**：

    - 所有同步中断必须设置超时，超时时间默认2000ms，禁止无限等待；

    - 异步中断绝对禁止阻塞VM主线程，必须在后台线程执行；

    - 中断处理完成必须释放GIL、清理任务、重置状态，避免资源泄漏。

---

## 四、测试规范

|测试类型|测试目标|核心用例|
|---|---|---|
|单元测试|验证单个模块的功能正确性|1. 对象池的内存复用、池满限流；2. 有界队列的添加、取出、超时清理；3. GIL的加解锁、超时释放；4. 同步/异步中断的基础逻辑|
|集成测试|验证模块间的流程闭环|1. VM创建→发起中断→挂起→唤醒→销毁的完整流程；2. 多VM并发发起中断的调度逻辑；3. 外设GIL的多通道负载均衡|
|压力测试|验证高并发下的内存安全、无空转|1. 10个VM并发发起中断，连续运行30分钟，无堆爆炸、无内存泄漏；2. 全异步中断场景下，CPU利用率无空转；3. 全同步中断场景下，无死锁、无超时泄漏 4.条件竞争|
|异常测试|验证极端场景的鲁棒性|1. VM中途崩溃，资源是否正常释放；2. 中断超时，GIL是否强制释放；3. 等待队列满，是否正确限流；4. 依赖异步中断时序的错误场景，是否能正确复现|
|性能测试|验证核心流程的性能|1. 同步中断的平均响应延迟；2. 异步中断的执行开销；3. VM调度的上下文切换耗时|
---

## 五、开发进度规划（单人练手版，总周期8周，可按需拉长）

### 前置说明

- 本规划适配**单人业余时间练手**（每周投入10~15小时），如果是全职开发，可压缩至4周完成；

- 每个阶段设置明确的里程碑、交付物、验收标准，必须完成当前阶段才能进入下一阶段；

- 每个阶段预留20%的缓冲时间，用于解决卡壳、调试bug。

|阶段|周期|核心目标|交付物|验收标准|
|---|---|---|---|---|
|阶段1：项目初始化与基础工具开发|第1周|搭建开发环境，完成所有基础工具模块开发，筑牢项目基石|1. CMake构建工程，可正常编译运行<br>2. 对象池模板类实现+单元测试<br>3. 有界等待队列实现+单元测试<br>4. 日志工具类实现|1. 工程可一键编译，无编译警告<br>2. 对象池能正确复用内存，池满正确限流，无内存泄漏<br>3. 有界队列能正确添加/取出任务，超时任务正确清理，超上限正确拒绝<br>4. 所有单元测试用例100%通过|
|阶段2：VM上下文与状态管理开发|第2周|实现VM的资源隔离、生命周期管理、挂起状态管理|1. VMContext结构体实现<br>2. VMManager管理类实现+单元测试<br>3. VMStateManager状态管理类实现+单元测试|1. VM可正常创建/销毁，资源限额生效，内存超限正确拦截<br>2. inList可正确标记VM挂起/运行状态，线程安全<br>3. 中断结果表可正确存储/读取返回值<br>4. 所有单元测试用例100%通过|
|阶段3：外设抽象与叫号模型开发|第3周|实现外设级GIL、多通道负载均衡、叫号等待/唤醒机制|1. PeriphGIL外设GIL类实现+单元测试<br>2. PeriphManager外设管理类实现+单元测试<br>3. 叫号模型的等待/唤醒逻辑实现|1. GIL可正确互斥，带超时加锁生效，无死锁<br>2. 多通道可正确负载均衡，通道间并行无干扰<br>3. 叫号模型可正确等待/唤醒任务，超时任务正确清理<br>4. 所有单元测试用例100%通过|
|阶段4：分级中断调度核心开发|第4周|落地核心设计，实现基于返回值的中断分级差异化调度|1. 中断类型枚举、中断任务结构体定义<br>2. 同步中断处理函数实现<br>3. 异步中断处理函数实现<br>4. 统一中断入口实现+单元测试|1. 同步中断可正确挂起VM，等待外设完成后返回结果，唤醒VM<br>2. 异步中断不阻塞VM主线程，后台等待VM空闲后执行<br>3. 可正确复现“依赖异步中断时序运行=程序错误”的场景<br>4. 所有单元测试用例100%通过|
|阶段5：VM调度器与核心流程闭环|第5周|实现VM调度器，打通VM创建→中断→调度→销毁的完整流程|1. VMScheduler调度器类实现<br>2. 完整流程Demo实现<br>3. 集成测试用例|1. VM可正常加入运行队列、挂起、唤醒，调度循环正常运行<br>2. 完整流程Demo可一键运行，无崩溃、无死锁<br>3. 多VM并发场景下，流程正常闭环，无逻辑错误<br>4. 所有集成测试用例100%通过|
|阶段6：压力测试与性能优化|第6周|解决堆爆炸、空转问题，优化核心流程性能|1. 压力测试用例实现<br>2. 内存泄漏/死锁问题修复<br>3. 核心流程性能优化<br>4. 限流告警功能实现<br>5. 压力测试报告|1. 10个VM并发运行30分钟，无堆爆炸、无内存泄漏、无死锁<br>2. 异步中断场景下，CPU无空转，利用率>80%<br>3. 同步中断平均响应延迟<500ms<br>4. 等待队列满时，正确限流，无异常崩溃|
|阶段7：异常处理与鲁棒性增强|第7周|处理极端场景，增强框架容错能力，贴合你之前的蜜罐防御设计|1. VM崩溃兜底逻辑实现<br>2. GIL超时强制释放逻辑完善<br>3. 异常场景的资源清理逻辑完善<br>4. 硬件优化建议功能实现<br>5. 异常测试报告|1. 单个VM崩溃不会导致整个框架崩溃，资源正常释放<br>2. 中断超时后，GIL正确释放，无永久挂起<br>3. 所有异常测试用例100%通过，无崩溃、无死锁<br>4. 频繁限流场景下，可正确打印硬件优化建议|
|阶段8：文档完善与项目收尾|第8周|完善项目文档，完成最终测试，项目正式交付|1. 完整开发手册<br>2. 用户使用手册<br>3. API接口文档<br>4. 最终全量测试报告<br>5. 最终可运行Demo|1. 文档完整、逻辑清晰，可指导二次开发<br>2. 最终Demo可一键编译运行，无编译警告、无运行异常<br>3. 所有测试用例100%通过<br>4. 项目代码符合开发规范，注释完整|
---

## 六、风险应对与进阶方向

### 6.1 开发风险应对

1. **模块开发卡壳**：优先回到最小Demo，先跑通核心逻辑再扩展，比如中断调度搞不定，先写单线程的同步/异步Demo，验证逻辑后再集成到框架。

2. **内存泄漏/死锁**：用valgrind工具检测内存泄漏、死锁位置，优先用智能指针、对象池规避内存问题，用RAII机制管理锁，避免手动加解锁导致的死锁。

3. **进度滞后**：优先保证核心流程闭环，砍掉非核心功能（比如硬件建议、复杂日志），先跑通“VM创建→同步中断→异步中断→VM销毁”的核心流程，再逐步优化。

### 6.2 进阶开发方向（完成基础版本后）

1. **CPU核绑定**：实现0核/1核的CPU亲和性绑定，0核跑调度器、1核跑外设管理，贴合你之前的蜜罐防御设计。

2. **蜜罐防御功能**：1核外设管理增加攻击检测逻辑，检测到异常后关停外设、切断网络，0核接管兜底。

3. **裸机移植**：剥离宿主OS依赖，将框架移植到QEMU模拟的ARM/x86裸机环境，向完整独立内核演进。

4. **跨架构支持**：增加ARM/RISC-V架构的适配，支持跨架构VM运行。

5. **可视化监控**：增加Web可视化界面，实时监控VM状态、中断调度、外设使用情况。

## 七、补充：生命周期时序图：
1. 同步中断完整流程

```Text
VM发起中断
→ InterruptScheduler.handle()
→ VMStateManager.set_suspend()
→ PeriphManager.wait_for_periph()
→ 获取GIL
→ 外设执行
→ 释放GIL
→ set_interrupt_result()
→ VMStateManager.set_suspend(false)
→ VMScheduler.wakeup_vm()
```

2. 异步中断完整流程

```Text
VM发起中断
→ InterruptScheduler.handle()
→ 后台线程排队
→ VM继续执行
→ VM空闲后
→ 外设执行
```

3. 锁顺序规范（强制）：
```Text
锁顺序规范：
1. VMState锁
2. VMContext锁
3. Periph GIL锁
4. WaitQueue锁
5. Scheduler锁
```

