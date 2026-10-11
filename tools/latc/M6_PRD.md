# AOT v2 M6：生产缺口修复与对照验证

## 目标

修复 M5 代码审查发现的信号并发、长期运行内存增长和共享状态并发访问问题，
补齐失效后重新验证、运行时 CPU 特性选择、缓存服务资源控制和真实动态程序覆盖，
并在相同条件下比较 AOT v2、旧 AOT、LATC M4 和可用的原生 LoongArch 结果。

## 必须完成的行为

- 信号恢复测试必须由两个真实线程同时执行 fault 恢复与模块卸载或失效，不能用
  标志位代替并发操作。测试检查 guest PC、通用寄存器、RFLAGS、向量状态、信号
  mask、altstack、嵌套信号和 sigreturn。
- host PC 诊断必须返回所属 AOT 模块身份和准确 guest PC；模块失效时不得读取已
  释放的 PC map 或实例。
- 反复 dlopen/dlclose、注册和失效不能使 runtime instance、registry snapshot、
  host module snapshot 或统计节点无限增长。释放方案必须允许无锁 dispatch 和
  异步信号读取安全结束。
- 所有跨线程发布的模块、实例和统计索引必须有明确的同步规则，并通过并发压力测试；
  支持时运行 ThreadSanitizer。
- mprotect 恢复执行权限以及可识别的 mremap 后，只有在重新读取映射身份、校验
  source digest 并完成新注册后才能恢复 AOT。系统调用失败不能无谓永久失效模块。
- 运行时按实际 LoongArch HWCAP 接受 BASE/LSX/LASX 模块，不能无条件假定 LASX。
- latcd 缓存有可配置容量限制和安全淘汰；编译并发度可配置且有上限；外部缓存加载
  消除校验到 dlopen 之间的可修改文件窗口，或明确拒绝 owner-writable 模块。
- 至少一个真实的大型动态程序完成冷缓存、暖缓存、长时间运行、DSO 反复加载和
  内存增长验证；报告模块级 AOT 命中与 JIT fallback。
- 性能报告使用同一 runner、输入、CPU、频率策略和缓存状态，分别测量 AOT v2、
  旧 AOT、LATC M4 和可获得的原生 LoongArch；每项至少 5 次并报告原始样本、中位数、
  几何平均、启动、注册、RSS/PSS 和跨进程共享页。

## 范围

`tools/latc` 内的 AOT v2 runner、registry、信号恢复、映射通知、latcd、测试程序、
SPEC/性能脚本和 M6 报告；在 `3a6000-25g` 上做 LoongArch 构建和验证。

## 不包含

- AOT 模块签名分发和系统级特权缓存。
- SPEC ref。
- 为匿名 guest JIT 代码生成 AOT。

## 完成标准

- 新增测试能在 M5 实现上明确暴露假并发或资源增长，并在修复后稳定通过。
- 本地快速测试、解析器测试、M1-M5 回归和 3A6000 真实 runner 回归全部通过。
- 长时间测试没有过期指针、数据竞争报告或随生命周期次数线性增长的保留对象。
- 真实动态程序和四组性能对照报告包含可复查的命令、摘要、原始数据和限制。
