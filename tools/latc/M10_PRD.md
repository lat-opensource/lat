# AOT v2 M10：混合缓存正确性与生产化结构

## 背景

M9 已在 `3a6000` 通过 Python、Git、SQLite 和 Redis 的 JIT、冷 AOT、暖 AOT、
fork 和故障降级测试，但旧环境曾稳定出现一个尚未解释的问题：guest loader 与
libc 同时使用 AOT、`dlopen` 插件使用 JIT 时，正式 fixture 返回 135
（`SIGBUS`）。当前源码已经包含这个混合组合的测试，必须先在当前 HEAD 和指定
机器上重新验证，不能把旧二进制或旧缓存的结果直接归因于当前实现。

主 LAT 与 `tools/latc` 还保存多份相同的 AOT v2 集成源码，`latc` 没有进入主
Meson 安装，同一缓存目录也缺少跨进程的单写者约束。这些问题会使修复遗漏、
安装版本不匹配或多个 latcd 同时修改缓存。

## 目标

1. 在 `3a6000` 解决或解释 loader+libc AOT、插件 JIT 的混合缓存 SIGBUS。
2. 让 dispatch、signal 恢复、失效和 fork 使用一致的模块实例身份与 generation。
3. 为 AOT v2 集成源码建立唯一来源，并自动检测不同步。
4. 用主构建生成并安装匹配的 runner、runtime、latcd 和 latc。
5. 同一缓存目录同时只允许一个 latcd 写入，并能从异常退出中恢复。
6. 使当前设计文档、复杂应用测试和实际代码行为一致。

## 验收标准

1. 空缓存、仅 loader、仅 libc、loader+libc 四种组合各执行至少 5 次，保留命令、
   退出码、stdout、stderr、模块 SHA256、runner build-id 和模块检查结果。
2. 混合组合在当前最终构建连续执行 20 次，全部输出正确且退出码为 0；signal、
   `dlclose`、invalidation 和 fork 回归通过。
3. AOT v2 集成代码只有一份规范实现，主 LAT 与隔离 runner 使用同一实现；故意
   制造不同步时检查必须失败并指出文件。
4. 一次主构建和安装产生四个匹配产物；从临时安装树完成冷编译和暖运行。
5. 第二个 latcd 不能写入已被占用的 cache；正常退出、异常退出和失效锁恢复测试
   通过，atomic current、profile merge 和 cache eviction 不退化。
6. 本地完整测试、TSAN、十万次 ELF 变异和 `3a6000` 动态/复杂应用回归通过。

## 范围限制

- LoongArch 构建和调试只在 `3a6000` 执行，不使用 `3a6000-25g`。
- SIGBUS 不能在当前 HEAD 复现时，不虚构代码根因；必须比较旧、新 runner、模块、
  rootfs 和配置，并把结论写入结果。
- 不改变 AOT v2 模块 ABI、现有 latc 命令行或 JIT 故障回退原则。
- 本阶段不实现持久编译队列、运行中 OSR、GUI、Wine、LSX-only 或增量编译。

