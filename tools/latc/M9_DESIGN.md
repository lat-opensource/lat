# AOT v2 M9 设计

## 复杂应用测试环境

测试脚本接收 `LATCD`、`LATC`、`RUNNER`、`RUNTIME_DIR`、`ROOTFS` 和 `WORKDIR`。
rootfs 由固定的 Debian 12 amd64 OCI 镜像或等价的固定包清单生成，并至少包含
Python、Git、SQLite 和 Redis。测试使用独立 HOME、cache、Unix socket、Redis
端口和临时目录；超时必须杀死完整进程组。

每个应用先在 JIT-only 模式执行，再用空缓存和 latcd 收集/编译模块，最后以
暖缓存新进程执行。冷编译失败不能阻塞应用。暖运行只要求功能正确和至少一个
AOT 模块命中，不要求全部文件 TB 命中。

## fork 生命周期

AOT v2 增加与现有 linux-user `fork_start()` / `fork_end()` 对应的 prepare、
parent 和 child 钩子。

- prepare 在现有全局排他区内阻止 AOT registry、ELF tracker、profile 和映射
  队列继续改变。
- parent 恢复原状态并按固定逆序释放锁。
- child 不复用父进程中的可变 AOT 状态。它停用 registry 查找，清空当前模块、
  FastTB 和线程目标缓存，丢弃待提交 profile/编译请求，并把 mutex、reader 和
  epoch 状态重建为单线程初始值。已映射的只读模块可留在地址空间，但子进程
  在 `execve()` 前只走 JIT。
- `CLONE_VM` 线程共享 registry；每个新线程使用空的线程局部目标缓存和当前模块。
- `execve()` 仍由新的 host LAT 进程启动，按正常入口重新发现并加载 AOT。

第一版不修改模块 ABI、profile v2 或 latcd protocol。

## 主 LAT 集成

先在 `tools/latc/lat` 中完成验证。通过后，把已记录在 `lat-local.json` 的 AOT v2
适配文件同步到主 LAT 路径，更新 Meson 源文件和 runtime library/latcd 构建目标。
`build-aot-v2-runner.sh` 保留为兼容和独立验证工具，但生产构建不再依赖 staging
源码覆盖。安装文档定义 runner、`liblat-aot-runtime.so.2`、latcd、缓存和 socket。
