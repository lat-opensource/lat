# AOT v2 M8：动态文件 ELF 覆盖率达到 100%

## 背景

M7 的动态 Bash 热运行只有 144 次 AOT 查找，另有 8473 次 JIT 回退和
6992 次运行时 TB 生成。进一步诊断显示，Bash native image 包含 64260 个
TB 和 39792 个 guest 地址重定位；当前 256 个 guest 地址槽经过依赖删除后，
模块只留下 3280 个 TB。现有模块还只保存原始 `cflags=0` 变体，动态运行进入
并行翻译后无法命中；现有 profile 仅记录 ASLR 后的绝对 PC，也不能用于稳定
重编译。

## 目标

使固定动态测试套件中所有实际执行的文件型 x86-64 ELF TB 都由 AOT v2
模块提供，同时保持静态 SPECint2000 train 的现有性能。

## 验收标准

1. dynamic hello、动态语义、反复 `dlopen`/`dlclose` 和 Bash 算术负载在
   profile 收集与重编译完成后的全新进程中满足
   `file_aot_dispatch_misses=0` 和 `runtime_file_tb_gen_attempts=0`。
2. 最终热运行不产生新的文件 ELF profile 记录；guest vDSO 和匿名可执行内存
   单独统计，不计入文件 ELF 覆盖率。
3. profile 请求的 TB 若未进入最终模块，编译必须失败并报告删除原因，不能
   静默发布不完整模块。
4. 在同一台 `3a6000-25g`、CPU 4、关闭 ASLR 条件下，12 项 SPECint2000
   train 交替运行 7 轮；候选版本相对补丁前版本的几何平均性能不低于
   `0.99x`，候选 AOT v2/M4 不低于 `0.95x`，12/12 输出正确且运行时 TB
   生成为零。
5. 保留 M6/M7 的信号恢复、失效、并发卸载、W^X、模块共享和缓存安全保证。

## 范围限制

- 100% 指固定测试路径中实际执行的文件型 ELF TB，不代表所有理论分支。
- guest vDSO、匿名 JIT 和自修改代码继续使用 JIT，但必须单独报告。
- 本任务使用 SPEC train，不运行 SPEC ref。
- 两级 guest 地址表最多支持每模块 65536 个不同的 guest RVA；超过时明确失败。
