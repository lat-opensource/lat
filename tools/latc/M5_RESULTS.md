# AOT v2 M5 生产测量与回归结果

## 结论

2026-08-26 在 `3a6000-25g` 上完成 M1-M5 回归和 SPECint2000 train。
所有功能测试通过，SPEC train 12/12 通过，每项少于 60 秒，未运行 ref。
固定 CPU 4 后，AOT v2 相对 LATC native-image 的逐项效率几何平均为
`97.27%`。这里的效率定义为 `native-image 时间 / AOT v2 时间 * 100%`。

## 环境和摘要

- Host: `zenglu-pc`，Loongson-3A6000，8 个逻辑 CPU，kernel
  `4.19.0-19-loongson-3`。
- SPEC 正式对比固定在 CPU 4：`taskset -c 4`。启动和内存样本没有固定
  CPU，是补充数据，不用于 SPEC 效率计算。
- Runner SHA-256:
  `0b0752b43a75368ea519e6807d637923dfbce2cd7cb42bd3075e8f7b192c64d3`。
- Runtime SHA-256:
  `93258dde9c88017cb54ebba33f1bccceda728863f5d5a8c740f70d7c951344ac`。
- `latc` SHA-256:
  `170a1d8db6db9158d5d9ad5c4e931dbfacb81e217a8d0e7daeec8374ca8519a8`。
- AOT v2 manifest SHA-256:
  `654360c318961ba0a9b86893e509eb36d83aeb1289d7c70ff9ba944fa04fc9ed`。
- native-image manifest SHA-256:
  `e57717f74fa673167d2d08084190eda694595ec0bb7835b2724c914d8edd5fe0`。
- AOT v2 使用提前生成的完整静态模块，属于暖模块状态；native-image 使用
  同一 exporter 生成的独立 LoongArch ELF。每组各运行一次完整的 12 项 train。

## SPECint2000 train

| benchmark | native-image (s) | AOT v2 (s) | 效率 | runtime TB |
| --- | ---: | ---: | ---: | ---: |
| 164.gzip | 9.958012 | 9.971816 | 99.86% | 0 |
| 175.vpr | 5.953737 | 6.065745 | 98.15% | 0 |
| 176.gcc | 1.109230 | 1.119759 | 99.06% | 0 |
| 181.mcf | 3.391621 | 3.654249 | 92.81% | 0 |
| 186.crafty | 6.230245 | 6.228659 | 100.03% | 0 |
| 197.parser | 2.149839 | 2.166509 | 99.23% | 0 |
| 252.eon | 2.072416 | 2.236724 | 92.65% | 0 |
| 253.perlbmk | 18.723730 | 19.705614 | 95.02% | 0 |
| 254.gap | 1.685529 | 1.805495 | 93.36% | 0 |
| 255.vortex | 2.995565 | 3.062145 | 97.83% | 0 |
| 256.bzip2 | 9.159324 | 9.162424 | 99.97% | 0 |
| 300.twolf | 3.701700 | 3.711108 | 99.75% | 0 |
| 几何平均 | - | - | 97.27% | 0 |

每项 `module_tbs == native_tbs`，因此这 12 个完整静态模块的已生成 TB
覆盖率为 100%；`runtime_tb_gen_attempts` 和 `runtime_tb_gen_calls` 全为 0。

## 启动、内存和动态覆盖

暖缓存动态 hello 启动样本为 `44851, 43977, 43886, 43839, 43693 us`，
中位数 `43886 us`，重复 5 次。

动态 TLS/IFUNC/version/interposition 用例运行时的单点 `/proc` 样本：

- RSS: `4080 kB`
- PSS: `1987 kB`
- Shared_Clean: `2144 kB`
- AOT 可执行映射: `1097728 bytes`，即 `67` 个 16 KiB 页、4 个映射
- `direct_targets=169`，`compat_tb_allocations=0`
- 7 条模块统计合计 `aot_lookups=169`、`jit_fallbacks=3091`，按这两个计数
  计算的动态 AOT 命中占比为 `5.18%`。loader/libc 是部分模块，出现 JIT
  fallback 符合 M2-M4 的既有要求。

## 完整回归

以下命令通过：

```sh
LD_LIBRARY_PATH=/home/zenglu/latc-m5-runner make test-aot-v2-m4 \
  RUNNER=/home/zenglu/latc-m5-runner/latx-x86_64 \
  AOT_V2_RUNNER=/home/zenglu/latc-m5-runner/latx-x86_64 \
  AOT_V2_RUNTIME_DIR=/home/zenglu/latc-m5-runner \
  X86_ROOTFS=/home/zenglu/loongrun-linux64-runtime/rootfs \
  SPEC_ROOT=/home/zenglu/spec2000-x64
```

通过项包括 `test-latcd-once`、`test-latcd-service`、
`test-latcd-runner`、static glibc、精确信号恢复、动态 ELF、100 次
`dlopen/dlclose`、动态语义以及 SPEC train 12/12。

正式 SPEC 对比命令在上述参数外增加 `taskset -c 4`。native-image 组还设置
`LATC_LA64_CC=gcc LATC_LA64_OBJCOPY=objcopy`，因为该主机只有原生
LoongArch binutils 名称。

## 模块摘要

主程序 SHA-256、TB 数和 PC map 数保存在 AOT v2 manifest。12 个模块的
SHA-256 如下：

```text
164.gzip    a05d14cb4dc7a36cc6169339865bbf07268fd07feae36911928e8593a2474ebe
175.vpr     16378eefa45dc20897ddc4bc10de12355255cee4775fa6cd00c0e4abc3fcf650
176.gcc     c67c19fadbd5cd63acaebd3364f9f1ba8e95fe3c2fe11832f987d7b1ea5dc1fe
181.mcf     198656d8705e33b983320ec5244c6a5fe8ef201c59b6a152e15c4d28188a95bf
186.crafty  66bbd1995bbe55b08aedb85bc322baf0858df228f5568022f542790a6dac69f9
197.parser  543f2c32ff04a2cce1208dedf5589d50dd5b2c9dee167424c7dbe46192a7c353
252.eon     7bd2da93f5868d2d86776cfe4b401b72eb9504627e4d16df36b5c7b6c93aaad4
253.perlbmk c66eb8dd5bd87df92b718c7a0fe126bbfd7f3a5f72305015ac31a9fefca5343a
254.gap     4d2cf4661104bd99718ca874cbdaf89395bb3d7acfa75f99e82419dee2456df3
255.vortex  14298fbae2c88d383703b2d5602e36704ef83db1a21086d9203f09af698074dc
256.bzip2   11aede877724b96d4b3b9b68fa1c8d707b9ddda6fea6cd741e9cd4e3fc95241c
300.twolf   4688f08fb497135251f26ce11b39c1816021abfdde4187c6e055c0b65dfa00f7
```

## 限制

- 当前 SPEC 树没有与这 12 个输入逐项对应的原生 LoongArch benchmark，
  因此“native”对照是 LATC native-image，不是手工编译的 LoongArch 程序。
- M1 和 M4 的旧结果保留在 `aot-v2/README.md` 和 `M4_TEST_PLAN.md`，但其
  runner 摘要和 CPU 绑定设置不同，不能与本次固定 CPU 4 的结果混成一组
  计算效率。
- RSS 和共享页是动态语义用例运行期间的单点样本；启动时间是 5 次样本，
  不是长期压力测试。
