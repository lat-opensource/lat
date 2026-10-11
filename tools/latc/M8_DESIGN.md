# AOT v2 M8 设计

## 稳定 TB 键和 TB 集合

模块 ABI 使用稳定键 `(source_sha256, guest_rva, semantic_flags)`。
`semantic_flags` 只包含 x86-64 code mode 和 parallel-safe 变体，不把 LAT 内部
`cflags` 直接写入永久文件格式。运行时在 AOT 查找前完成映射；编译器在生成
native image 时对 TB 及 TB target relocation 使用相同映射。

TB 集合是固定结构的二进制 `LATTBKS` 文件。文件头包含 source SHA-256、记录数
和序号，每条记录包含 `RVA`、`FLAGS` 和必须为零的保留字段。记录按
`(RVA, FLAGS)` 排序并做集合合并，不记录执行次数。编译器只接受这一种格式。

运行时不在 AOT 查找或 TB 执行路径计数。每个文件 TB 完成 JIT 翻译时，把已经
知道的 PC 和 flags 转换为 source SHA、RVA 和 semantic flags，并加入进程内集合。
进程正常退出时提交集合，不扫描全局 JIT TB 表。

## 两级和三级 guest 地址表

旧格式把每个 guest 地址直接放在 `$fp` 之前，LoongArch `ld.d` 的立即数范围
将其限制为 256 个槽。新模块使用最多 256 个一级页指针，每页保存 256 个
guest 地址：

1. 第一条 `ld.d` 从 `$fp` 相对槽读取页指针。
2. 第二条 `ld.d` 从页内读取 `guest_load_bias + guest_rva`。
3. 多余的重定位保留指令改为 NOP。

`LatAotGuestSlotV2.fp_offset` 保存一级页指针槽，原 `reserved` 字段在新模块标志
存在时保存页内字节偏移。旧模块没有该标志，继续使用原平面表。页号按排序后的
guest RVA 确定，保证相同输入生成相同模块。

T-318 增加三级格式。超过 65536 个地址时，第二条 `ld.d` 读取叶页指针，第三条
`ld.d` 再读取 guest 地址；容量提高到 16777216。`reserved` 高 16 位保存中间页
偏移，低 16 位保存叶页偏移。小模块仍使用两级格式，避免增加一次内存读取。

打包器对删除原因分别计数。任何 TB 集合请求键在依赖删除后缺失都使
`compile-module` 失败；未执行的 CFG TB 仍允许形成部分模块。

## TB 集合合并与模块发布

latcd 协议只接受同时携带 source FD 和只读 TB 集合 FD 的请求。latcd
重新计算 source digest，验证 RVA 位于可执行 `PT_LOAD`，再将记录合并到
`.tbsets/<source-sha>.tbset`。同一 source 同时只有一个任务；集合未增加时不
重新编译。编译期间收到新记录时，完成后最多追加一个任务。

模块名为 `<source-sha>-<tbset-digest>.so`，`<source-sha>.current` 通过原子
rename 指向当前不可变模块。没有有效 current 就按缓存未命中处理。当前进程不
热替换已经注册的模块。

## 格式和安全

- native image 内部格式升级，模块 ABI 通过新 module flag 扩展，不改变旧描述符
  的字段大小。
- TB 数量、RVA、flags 和 FD 类型全部验证；每 source 最多 1048576 个键。
- current 和 TB 集合使用临时文件、`fsync`、原子 rename，目录保持当前用户
  私有权限。
