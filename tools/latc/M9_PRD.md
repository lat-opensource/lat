# AOT v2 M9：复杂 Linux CLI 与服务应用

## 背景

M8 已证明固定的动态 hello、Bash 和 `dlopen` 测试在 profile 收敛后可做到
文件 ELF 零 JIT，但尚未用 Python、Git、SQLite 和 Redis 验证完整的进程、线程、
文件锁、网络和服务生命周期。AOT v2 runner 仍通过临时 LAT 源码树构建，
主 LAT 构建尚未直接包含这些运行时代码。

## 目标

在 3A6000/LASX 上，使 Debian 12 amd64 的 Python、Git、SQLite 和 Redis 在
JIT-only、冷 AOT 和暖 AOT 条件下功能正确；补齐多线程 `fork()` 后的 AOT 状态
处理。隔离 runner 验证通过后，将 AOT v2 runner 纳入主 LAT 构建与安装路径。

## 验收标准

1. 固定 Debian 12 rootfs 可重复生成，记录镜像或包版本与内容摘要。
2. Python、Git、SQLite 和 Redis 的功能用例在 JIT-only 和暖 AOT 下各连续通过
   三次；Python、SQLite 和 Redis 的并发循环各运行 60 秒。
3. 多线程进程 `fork()` 后，子进程不继承不安全的 AOT reader、锁、线程局部
   上下文或跳转缓存；子进程可继续 JIT，`execve()` 后可重新加载 AOT。
4. latcd 缺失、编译失败、损坏 current/module 和只读缓存时，应用正确回退 JIT，
   不崩溃、不死锁、不使用陈旧代码。
5. AOT v2 runner 可由主 LAT 的 Meson 构建直接生成，不再要求复制并修改临时
   LAT 源码树；安装布局和 per-user latcd 启动方法有文档和验证。
6. 本地完整测试、TSAN、十万次 ELF 变异，以及 3a6000 的信号、失效、并发
   `dlclose` 和动态语义回归通过。

## 范围限制

- 功能正确优先，允许文件或匿名代码回退 JIT，不要求零 JIT。
- 只支持 3A6000/LASX；不实现 LSX-only 产物。
- 不包含 GUI/Electron、Wine/Windows、运行中热替换、OSR、SPEC ref 或性能提升门槛。
- 所有 LoongArch 构建、调试和功能验收只在 `3a6000` 执行。
