# AOT v2 M9 验证结果

## 固定测试环境

- 主机：`3a6000`，LoongArch 3A6000/LASX。
- rootfs：Debian 12 amd64，镜像
  `debian@sha256:6ebd97fa83deb272194a2cf015b3d26a4d538e9ad3a7a79d544c8af5b0a01443`，
  snapshot `20260824T000000Z`。
- rootfs 文件树 SHA256：
  `dc716a7f65169ae1c2b1acafe0f676b4b267c27911faab3ddfbbe7ddb9040be9`。
- 主要包：Python 3.11.2、Git 2.39.5、SQLite 3.40.1、Redis 7.0.15、
  OpenSSL 3.0.20。

rootfs 的包清单、主要二进制摘要、全部文件/符号链接清单和文件树摘要均由
`scripts/prepare-complex-rootfs.sh` 生成在相邻 `metadata` 目录。

## 功能和稳定性

自动测试覆盖以下真实操作：

- Python：`ssl`、`sqlite3`、`ctypes`、signal、8 个线程、文件 I/O 和嵌套子进程。
- Git：提交、分支、合并、repack、gc、`fsck --strict` 和最终 tree hash。
- SQLite：WAL 模式下 4 个进程并发写入 1000 行，核对行数、求和和
  `integrity_check=ok`。
- Redis：4 个并发客户端、事务、pub/sub、BGSAVE、关闭和 RDB 重启核对。

隔离 runner 的 JIT、冷 AOT 和暖 AOT 均通过。JIT 压力执行 8 轮、60 秒；暖 AOT
执行 4 轮、65 秒。主 LAT runner 的冷/暖矩阵通过，最终暖 AOT 压力测试执行
4 轮、65 秒。
每轮都完整重跑四个应用，因此超过三轮连续通过要求。

复杂应用暴露并修复了 SQLite 锁问题：对普通数据库映射执行 `dup` 后再 `close`
会释放该进程的 POSIX `fcntl` 锁。现在只为只读、非匿名的 x86-64 ELF 映射复制
描述符，普通数据库和可写映射不再进入 AOT ELF 跟踪。

## fork 和故障降级

真实多线程 fork fixture 通过。父进程继续命中 AOT；子进程清空当前模块和跳转
缓存、关闭继承的源文件描述符、重建锁，并在 exec 前只使用 JIT。测试输出明确
包含 `AOT v2 fork child switched to JIT`。

Python 金标准与以下五种故障下的输出逐字相同：

1. latcd socket 不存在；
2. latcd 调用的编译器退出失败；
3. `.current` JSON 损坏；
4. 模块 ELF 头损坏；
5. 缓存目录只读。

编译器失败统计为 `compiled=0 failed=1`；只读缓存统计也为
`compiled=0 failed=1`。五种情况下均记录 `module=missing` 和非零 JIT fallback，
没有崩溃、死锁或遗留测试进程。

## 主 LAT 构建

从空构建目录配置主 LAT，Meson 直接生成：

- `latx-x86_64`：
  `68ffab88f6eff52874f99c9ece721e71673f538cfe081dccf3b0545971353072`；
- `liblat-aot-runtime.so.2`：
  `cab0f0ab4787036981f9fbaf64ddcaf0de7eccda4984b915bc824fc30e700c35`；
- `latcd`：
  `58737fe32da22756cc5e4dd42f005df604abb574e8acd803e8b142dddff9c591`。

runner 的 ELF `NEEDED` 包含 `liblat-aot-runtime.so.2`，构建目录 RPATH 为
`$ORIGIN/`。主构建产物重复通过复杂应用矩阵、fork、`dlopen`、代码失效和动态
链接语义测试。共享库失效夹具改为 `-nostdlib`，并使用显式退出入口，避免 CRT
启动代码制造与待测函数无关的缺失 TB。

## 本地回归

- `make -C tools/latc test`：通过；
- `make -C tools/latc test-aot-v2-tsan`：通过；
- AOT ELF 变异：100000 次，通过，固定 seed `0x4c4154414f543256`；
- `git diff --check` 和 shell 语法检查：通过。

## 限制

本阶段只承诺 3A6000/LASX 上的复杂 Linux CLI 和服务应用。文件或匿名代码仍可
安全回退 JIT；未覆盖 LSX-only、GUI/Electron、Wine/Windows、运行中热替换、
OSR、SPEC ref 和性能提升门槛。`latc` 编译器仍由 `make -C tools/latc` 构建，
主 Meson 已直接构建 runner、运行库和 latcd。
