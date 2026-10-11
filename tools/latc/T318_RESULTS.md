# T-318 结果：Git 基本完全 AOT

## 根因和修改

Git 原始 native image 包含 178711 个 TB、120708 个 guest 地址重定位和 74121 个
不同 guest 地址。旧两级表只能表示 65536 个地址；溢出先删除 9333 个 TB，再通过
直接跳转和顺序执行依赖连带删除，最终模块只剩 16398 个 TB。

新增三级 guest 地址表后，Git 模块保留全部 178711 个 TB、663866 条 PC map 和
74121 个 guest 地址。模块标志为 23，包含三级表标志。打包器同时改用 guest RVA
哈希索引，避免大模块上线性扫描。

覆盖率验收改为统计所有文件型 ELF，包括 `module=missing`。冷采集允许一次进程提交
全部依赖模块，避免主程序反复占用唯一提交名额而 libc 永远没有 profile。

## 3A6000 结果

clean build/install：

- source：`/home/zenglu/latc-t318-source`
- build：`/home/zenglu/latc-t318-build`
- install：`/home/zenglu/latc-t318-install`
- build identity：`a203ec4d7db5ddfd764fd57e59cb3ecc677514b78b18f24f0b1a069f4a45abc7`
- 429 步构建和安装完成；`test-aot-v2-install.sh` 通过。

稳定缓存：`/home/zenglu/latc-t318-git-train/cache`。最终 latcd 统计为
`requests=180 queued=10 deduplicated=102 cache_hits=72 compiled=10 failed=0`，队列和
活动任务均为 0。

离线暖运行：`/home/zenglu/latc-t318-git-warm2`。18 个 Git 进程合计
`aot_lookups=40800`、`jit_fallbacks=0`，覆盖率 100%。Git 主程序、动态链接器、
libc、libz 和 libpcre2 全部为 `module=registered`；`compiler_submissions=0`、
`compiler_submission_failures=0`。

## 性能

`/home/zenglu/latc-t318-git-performance/performance.json` 在同一构建上做 5 轮交替
JIT/暖 AOT：

| 轮次 | 顺序 | JIT 秒 | 暖 AOT 秒 | 暖/JIT |
|---|---|---:|---:|---:|
| 1 | JIT→暖 | 4.790 | 3.341 | 0.697413 |
| 2 | 暖→JIT | 5.011 | 3.437 | 0.685927 |
| 3 | JIT→暖 | 4.818 | 3.403 | 0.706256 |
| 4 | 暖→JIT | 4.855 | 3.382 | 0.696587 |
| 5 | JIT→暖 | 4.820 | 3.453 | 0.716445 |

五轮均无失败，暖 AOT 比 JIT 快约 28% 至 31%。性能阶段使用稳定 cache，前后
SHA256 清单一致，没有编译请求。

## 限制

100% 结论只适用于这组固定 Git 命令及已采集的 ELF 路径。新的插件、不同 locale、
不同配置或新输入可能执行新 TB，必须先采集并重新生成模块。匿名代码和动态生成代码
不属于文件型 ELF 覆盖率，继续单独计数。
