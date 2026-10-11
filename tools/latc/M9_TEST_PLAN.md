# AOT v2 M9 测试计划

## 应用功能

- Python：导入 `ssl`、`sqlite3`、`ctypes`、`subprocess`、`threading`；8 线程计算、
  文件 I/O、子进程和信号处理。
- Git：init、commit、branch、diff、merge、repack、gc、`fsck --strict`，并核对最终
  tree hash。
- SQLite：WAL 模式，4 个并发进程读写，最终 `integrity_check` 为 `ok`，行数和
  校验和正确。
- Redis：4 个并发客户端覆盖 SET/GET、事务和 pub/sub；执行 `BGSAVE`，关闭并从
  RDB 重启后核对键值。

每项在 JIT-only 与暖 AOT 模式各连续执行三次。Python、SQLite 和 Redis 的并发
循环各运行 60 秒。冷缓存+latcd 单独验证异步编译与后续消费。

## fork 与降级

- C fixture 在 AOT 主线程和多个辅助线程存在时 fork；子进程执行 guest 代码、
  创建线程、处理信号，再 exec 一个暖缓存程序。
- 断言父进程继续命中 AOT；fork 子进程在 exec 前只走 JIT，且没有继承 reader、
  retired snapshot、当前模块或目标缓存。
- 分别测试 latcd socket 缺失、编译器失败、current JSON 损坏、module ELF 损坏和
  缓存只读；应用输出与 JIT-only 金标准一致。

## 回归和执行环境

- 本地：`make -C tools/latc test`、`make -C tools/latc test-aot-v2-tsan`。
- 3a6000：M1-M8 runner、signal、invalidation、dynamic semantics、真实并发
  `dlopen/dlclose`，以及新的复杂应用测试。
- 主 LAT 集成后从干净构建目录重配重建，再重复复杂应用矩阵。
- LoongArch 构建和调试只在 `3a6000`；压力时长默认 60 秒。
