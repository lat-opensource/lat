# AOT v2 M10 设计

## 诊断顺序

使用现有 `test-aot-v2-dlopen-runner.sh` 作为正式入口。它已经构造 loader 与 libc
AOT、插件 JIT 的混合缓存。先从干净源码和构建目录生成 runner、runtime、latc、
fixture 和模块，再执行四种缓存组合。每次记录模块内容和运行器身份，避免把旧
缓存加载问题误判为运行时缺陷。

若失败，按以下顺序区分原因：

1. 宿主故障 PC 属于 AOT text、JIT code cache、runtime helper 或普通宿主代码；
2. signal 使用的当前实例是否与宿主 PC 所属模块相同，generation 是否仍有效；
3. 插件反复映射和卸载后，访客执行区间及跳转缓存是否已失效；
4. module note 中的 source digest、codegen-id、CPU feature 和 TB 集合是否与实际
   loader/libc 匹配。

只有当前 HEAD 的正式 fixture 失败才修改运行时代码。测试必须先稳定失败，再进行
修复；调试日志统一使用可搜索前缀，并在提交前删除。

## 运行时状态

保留现有外部 runner hook。内部把已加载模块、guest 实例、host text 范围、
generation 和 PC map 作为同一生命周期记录发布。dispatch、signal 恢复和失效
不得分别构造互不关联的模块身份。fork parent 保留状态；fork child 在 exec 前
停用继承的 AOT 状态、清空当前模块和跳转缓存、关闭 latcd 连接并只走 JIT；
exec 替换宿主进程，由新进程初始化空 registry 后重新发现模块。

source 指完整 x86 ELF 及其 SHA256；module 指一个不可变 AOT ELF；instance 指
module 在某个 guest load bias 的一次映射。instance 注册时 generation 从 1 开始；
失效时先从 range snapshot 移除，再清 active、增加 generation 并清理缓存目标。
dispatch 和 signal PC 恢复都必须同时匹配 instance、generation 和翻译标志。
TB 集合是按 source 合并的 `(RVA, FLAGS)` 集合，不记录执行次数；current 是通过临时文件、只读权限、文件
fsync、原子 rename 和目录 fsync 发布的小型 JSON 索引。JIT fallback 表示无法
证明有有效 AOT target 时继续普通 LAT 翻译，不表示运行中热替换 module。

## 源码唯一来源

保留 `tools/latc` 的最小 imported LAT 树，不复制完整仓库。主 LAT 路径是 AOT v2
集成实现的规范位置。`aot-v2-source-map.json` 记录规范文件与生成副本的关系，
同时覆盖主 LAT、`tools/latc/lat` imported 树、AOT runtime 和 native 公共头文件；
隔离 runner staging 读取同一清单。`lat-local.json` 只标记 imported 树中的生成
文件。只有 `latc-verify` 和 AOT v2 stub 等主 LAT 没有的独立文件继续在 imported
树维护。`sync-aot-v2-sources.py` 可重复生成副本，`check-import` 强制逐字节一致；
检查失败同时打印生成文件和规范来源，禁止把手工修改后的副本带入构建。

## 构建与安装

主 Meson 负责四个匹配产物：`latx-x86_64`、`liblat-aot-runtime.so.2`、`latcd`
和 `latc`。安装测试使用临时 prefix，不引用源码树 build 目录。latcd 启动或调用
编译器时检查 codegen/build 身份，不匹配时返回明确错误，应用继续 JIT。

## 缓存单写者

latcd 打开 cache 后立即取得跨进程独占锁，并在整个服务期持有描述符。锁由内核
在进程退出时自动释放，不用 PID 文件判断进程是否存活。第二个 latcd 获取失败时
在创建临时文件、合并 TB 集合或清理 cache 前退出。现有临时文件、`rename()`、
文件和目录同步顺序继续保留。

## 测试调度和文档

复杂应用每个阶段使用独立目录；每个 guest 命令通过 `setsid` 取得独立 session
和进程组，并使用该阶段自己的 HOME。阶段目录内的 `commands.jsonl` 逐条记录
环境、argv、stdout/stderr 路径和整数退出码，`result.json` 记录阶段、应用清单、
迭代次数和最终结果。cold 和 warm 阶段都在进入下一阶段前等待 latcd 队列及 worker
清空，故障测试不会复制正在发布的 cache。stdout 和 stderr 继续按应用单独保存。
共享设计文档固定 source、module、instance、generation、TB 集合、current 和 JIT
fallback 的含义，并在实现变化后同步更新。

## T-316：暖缓存性能

冷运行不为刚发现的 ELF 编译空的基础模块。进程正常退出或全局清空 TB 前，运行时
扫描已经存在的 JIT TB 表，按 source 生成 TB 集合并提交。latcd 按 source 合并集合；
集合没有新增项时不重新编译。同一 source 最多保留一个等待任务；编译线程读取集合
时先复制不可变快照。若编译期间集合增加，服务只补排一次任务。

暖运行用 ELF 的设备号、inode、大小、mtime 和 ctime 查找 latcd 发布的 SHA256，
不再为每次进程启动重新读取大文件。没有活动 AOT 模块时，普通运行不再为每个 TB
扫描模块统计；已确认不命中的 `(pc,cflags)` 按注册表代数缓存在 TB 跳转缓存中，
模块注册、停用或 `fork()` 后自动失效。
