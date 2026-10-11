# AOT v2 M6 测量与回归结果

## 结论

2026-08-27 在 `3a6000-25g` 上完成 M1-M6 功能回归和四模式性能对照。
所有功能测试通过，SPECint2000 train 12/12 通过。性能对照包含 12 个程序、
4 种模式、每种模式 5 个正式样本，共 240 个有效样本。

本次结果没有显示 AOT v2 性能提升。以真原生 LoongArch 时间除以各模式时间
计算效率，12 项几何平均为：

- M4：`67.12%`
- AOT v2：`42.47%`
- 旧 AOT：`64.78%`
- 真原生：`100.00%`

AOT v2 相对 M4 的几何平均速度比为 `0.6328x`，相对旧 AOT为
`0.6557x`。数值小于 1 表示 AOT v2 更慢。

## 测试环境

- Host：`zenglu-pc`，LoongArch64 3A6000，kernel
  `4.19.0-19-loongson-3`。
- Runner：`/home/zenglu/latc-m6-runner/latx-x86_64`，SHA-256
  `b90e234e8cb6d71a76795d92727b59161ba8223c8712062795cf400b55cd7d1f`。
- SPEC 输入：`train`，所有正式样本固定到 CPU 4，ASLR 保持系统默认。
- 该内核没有提供 CPU 4 的 `cpufreq` sysfs 信息，因此报告中的
  `cpu_frequency_settings` 为空；四种模式仍在同一机器、同一 CPU 和同一
  连续测试过程中轮换运行。
- M4 和 AOT v2 产物均在测量前用上述 M6 runner 重新生成。报告记录了每个
  guest、M4、AOT v2 和真原生文件的 SHA-256。
- 旧 AOT 为每个程序使用独立 `HOME`。先生成非空 `.aot2` 文件，再做一次
  热缓存验证，最后才记录正式样本。没有使用用户的 `~/.cache/latx`。

完整机器可读数据保存在
[`spec2000/M6_SPEC_COMPARE.json`](spec2000/M6_SPEC_COMPARE.json)。它包含
240 个正式样本、每项中位数/均值/最小值/最大值/变异系数、缓存文件大小和
全部输入摘要。远端目录还保存 288 个 SPEC `.raw` 文件，其中包括正式样本
和预热样本。

## SPECint2000 train 中位数

| Benchmark | M4 (s) | AOT v2 (s) | 旧 AOT (s) | 真原生 (s) | AOT v2 效率 |
|---|---:|---:|---:|---:|---:|
| 164.gzip | 9.972524 | 10.813144 | 9.908709 | 9.450136 | 87.39% |
| 175.vpr | 5.972930 | 7.587499 | 6.033909 | 4.696426 | 61.90% |
| 176.gcc | 1.109692 | 3.103719 | 1.142582 | 0.651832 | 21.00% |
| 181.mcf | 3.623085 | 4.295834 | 3.655775 | 3.438119 | 80.03% |
| 186.crafty | 6.253299 | 10.346088 | 6.242261 | 3.528326 | 34.10% |
| 197.parser | 2.158505 | 2.852369 | 2.109927 | 1.529639 | 53.63% |
| 252.eon | 2.089173 | 6.634004 | 2.240177 | 1.264683 | 19.06% |
| 253.perlbmk | 18.963919 | 33.361572 | 20.929910 | 9.518451 | 28.53% |
| 254.gap | 1.679645 | 2.934425 | 1.806854 | 1.226287 | 41.79% |
| 255.vortex | 3.056388 | 5.436355 | 3.708786 | 1.576076 | 28.99% |
| 256.bzip2 | 9.159918 | 10.483618 | 9.091335 | 5.355079 | 51.08% |
| 300.twolf | 3.710785 | 4.537706 | 3.642413 | 2.795072 | 61.60% |

## 启动、注册、内存和共享页

动态真实应用使用 x86-64 `/bin/bash`、动态加载器、libc 和 libtinfo，共提交
并编译 4 个 ELF 模块。详细数据见
[`M6_DYNAMIC_RESULTS.md`](M6_DYNAMIC_RESULTS.md)。

- 空缓存并由 `latcd` 编译：`8653 ms`。
- 4 个 AOT 模块已缓存：`10265 ms`。暖运行没有加速。
- 4 个模块的注册时间：`2 us`、`111 us`、`91 us`、`175 us`。
- 暖运行只有 141 次 AOT 命中，另有 9519 次 JIT 回退；运行时生成 TB
  7772 次。该部分覆盖率不足解释了暖运行仍然偏慢。
- 16.35 秒长运行采集 133 个样本：RSS `31984-44112 KiB`，PSS
  `28736-40532 KiB`。最后 20 个样本的 RSS 和 PSS 各只变化 `192 KiB`，
  本用例未显示持续线性增长。
- 两个进程映射同一组 4 个模块时，合计 RSS `9328 KiB`、PSS
  `5167 KiB`、Shared_Clean `8208 KiB`，证明本次运行共享了干净的 AOT
  模块页。

## 回归结果

远端完整回归命令通过：

```sh
make test-aot-v2-m4 X86_CC=false \
  RUNNER=/home/zenglu/latc-m6-runner/latx-x86_64 \
  AOT_V2_RUNNER=/home/zenglu/latc-m6-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/home/zenglu/latc-m6-runner \
  X86_ROOTFS=/home/zenglu/loongrun-linux64-runtime/rootfs \
  SPEC_ROOT=/home/zenglu/spec2000-x64
```

通过项包括 `test-latcd-once`、`test-latcd-service`、
`test-latcd-runner`、static glibc、信号恢复、动态 ELF、
`dlopen`/`dlclose`、动态语义以及 SPECint2000 train 12/12。

Loongnix 没有 x86 交叉编译器，回归使用本机 x86 编译器生成测试程序后同步
到远端；`latc`、AOT v2 runtime、模块生成和所有运行测试仍在 3A6000 上执行。

本机以下命令也通过：

```sh
make -C tools/latc test
make -C tools/latc test-aot-v2-tsan
```

其中 ELF 变异测试运行 100000 次，registry 的 ThreadSanitizer 测试未报告
线程数据竞争。

## 限制和后续工作

- 当前数据证明功能正确和内存增长受限于本次工作负载，不能证明所有真实应用
  都不会增长。
- 动态 Bash 暖运行比冷运行慢，SPEC 的 AOT v2 几何平均效率也低于 M4 和旧
  AOT。M6 不能宣称性能提升；需要单独分析模块查找、间接跳转和未覆盖 TB 的
  开销。
- SPEC 使用 train 输入，不是耗时更长的 ref 输入。
- 内核没有暴露 `cpufreq` 设置，无法在报告中记录 governor 和频率上下限。
