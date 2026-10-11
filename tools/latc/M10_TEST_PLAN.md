# AOT v2 M10 测试计划

## 混合缓存 SIGBUS

- 在 `3a6000` 从干净目录构建当前 HEAD，不复用旧目标文件或旧模块。
- 使用正式 dlopen fixture 分别执行空缓存、仅 loader、仅 libc、loader+libc。
- 四种组合各至少 5 次；混合组合最终连续 20 次。
- 每次保存退出码、stdout、stderr、runner/runtime/latc SHA256 与 build-id、guest
  loader/libc/plugin SHA256、模块 note/descriptor 摘要和 cache 文件清单。
- 失败时捕获宿主 PC、线程、寄存器、栈、`/proc/PID/maps` 与 AOT host index；
  GDB 若改变 guest signal 时序，则改用最小 signal-safe 诊断和 core/ptrace 辅助。

## 运行时回归

- `test-aot-v2-dlopen-runner`：混合、全热、无 ASLR 和 signal/invalidation race。
- signal、dynamic runner、dynamic semantics、invalidation、fork。
- 本地 `make -C tools/latc test`、TSAN registry 和固定 seed 的 100,000 次 ELF 变异。

## 源码与安装

- imported source 和规范 AOT v2 文件一致性检查。
- 故意修改生成副本的临时测试必须失败，并指出不一致文件。
- 主 Meson 干净构建四个产物，临时 prefix 安装后检查 ELF `NEEDED`、RPATH/动态
  链接、版本身份和文件权限。
- 从安装树启动 latcd，执行冷缓存 fixture，再用新进程暖运行并核对输出和 AOT
  命中。

## cache 所有权和故障恢复

- 两个不同 socket 的 latcd 同时使用同一 cache；第二个必须在写入前失败。
- 第一个正常退出和 `SIGKILL` 后分别启动新实例，均能重新取得锁。
- 并发相同/不同 source、profile merge、negative cache、queue full、cache eviction、
  损坏 current/module 和只读 cache 回归通过。

## 复杂应用

- Python、Git、SQLite WAL 和 Redis BGSAVE/restart 在 JIT、冷 AOT、暖 AOT 下通过。
- 每个阶段使用独立目录和独立 `HOME`。每个 guest 命令由 `setsid` 建立新的进程组，
  并向该阶段的 `commands.jsonl` 写入命令、环境、整数退出码、stdout/stderr 路径和
  `new_process_group=true`。每个阶段另写 `result.json`，记录应用、迭代次数、耗时和
  阶段退出码。
- Redis 启动探测允许在服务就绪前返回非零；这些尝试同样记录在 `commands.jsonl`，
  但必须在阶段最终成功前得到 `PONG`。冷、暖阶段结束后必须等待 latcd 的
  `active_jobs=0`、`queue_depth=0`，才可进入下一阶段或故障测试。
- latcd 缺失、编译失败、current 损坏、module 损坏和只读 cache 时输出与 JIT 金
  标准一致，无崩溃、死锁或遗留进程。

## T-316 暖缓存性能

- 在同一台 `3a6000`、同一 release 安装树和同一复杂应用 rootfs 上测试。
- 冷运行结束后确认 Python、Git、SQLite、Redis server 和 client 都有有效
  `current`，latcd 的 `failed=0`，再复制为只读测试来源。
- JIT 与离线暖 AOT 交替执行至少 5 轮，奇数轮先 JIT、偶数轮先暖 AOT；每个应用
  用 `CLOCK_MONOTONIC` 纳秒值计时，并保存各轮原始 stdout、stderr 和 JSON。
- 每一轮要求暖 AOT 的四个应用总时间严格小于配对 JIT；性能阶段不启动 latcd，
  cache 内容测试前后 SHA256 必须相同，记录 `compiler_requests=0` 和
  `compiler_failures=0`。
