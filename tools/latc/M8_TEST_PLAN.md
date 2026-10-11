# AOT v2 M8 测试计划

## 快速失败测试

1. 构造含 257 个以上 PIE guest 地址的 native image，证明旧打包器删除 TB；
   修复后检查 profile 请求 TB 全部进入模块。
2. 同一 RVA 分别请求普通与 parallel 变体，最终模块必须有两个不同 TB 键，
   运行时分别命中。
3. ASLR 改变后复用同一 source profile，RVA 必须保持稳定。
4. protocol v2 并发提交重叠 profile，最终 canonical profile 只保留唯一键并累加
   count；损坏、越界和超限输入必须拒绝。

## 动态覆盖率

在 `3a6000-25g` 使用隔离 HOME、cache 和 socket，开启 ASLR，运行：

- dynamic hello；
- TLS、IFUNC、符号版本、interposition 和 `LD_PRELOAD` 动态语义；
- 反复 `dlopen`、回调、signal、`dlclose` 和再次加载；
- `/usr/bin/bash` 的 250000 轮算术负载。

每项最多执行三轮“运行、profile 合并、latcd 重编译”。最后用全新进程运行并
检查：输出正确、所有实际执行的文件 ELF 已注册、
`file_aot_dispatch_misses=0`、`runtime_file_tb_gen_attempts=0`、没有新增 profile
键。非文件 TB 单独打印，不作为失败。

## 回归

- `make -C tools/latc test`
- `make -C tools/latc test-aot-v2`
- `make -C tools/latc test-aot-v2-tsan`
- 远端 `make test-aot-v2-m4 ...`
- ELF 变异测试 100000 次
- 信号恢复、真实并发卸载、失效、fork/exec、只读共享页和资源测试

## SPEC 性能

保留补丁前 runner、模块和摘要；候选使用独立构建。固定
`3a6000-25g` CPU 4 并关闭 ASLR，对 12 项 SPECint2000 train 交替执行基线、
候选和 M4，每种模式 7 轮。每项使用中位数，最后计算几何平均；拒绝输出错误、
runtime TB 非零、候选/基线低于 `0.99x` 或候选/M4 低于 `0.95x`。
