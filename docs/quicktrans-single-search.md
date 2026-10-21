# LATU Quicktrans 只编码一份故障恢复表

Quicktrans 固定 210fe41，LATU 基于 cfbcf5b 加此前 envcache 补丁。本次只改 LATU accel/tcg/translate-all.c，不修改旧路径指令翻译函数，也不改 Quicktrans 仓库。

## 原调用及修改

原 Quicktrans 返回机器码长度加内部恢复表长度。外层在该总长度之后再次调用 encode_search，tb->tc.size 也设为该总长度，因此故障恢复读取的是第二份表。

当 IS_QUICKTRANS_TB 已设置时，使用 tcg_ctx->gen_insn_end_off[tb->icount-1] 取得实际机器码长度。Quicktrans 在 terminal 路径保存该终止指令及退出代码的结束偏移；在普通退出路径追加出口后更新最后一项。以原返回总长减去该值获得内部表长，然后将 gen_code_size 设为实际机器码长度。tb->tc.size 指向第一份表开头；后续 code_gen_ptr 的对齐分配仅计算机器码和一份表。

非 Quicktrans 路径仍按原位置调用 encode_search。Quicktrans 内部既有 highwater 检查保留；成功标志只在发码和表编码成功后设置，失败仍走原失败/回退流程。没有删掉恢复列或修改表格式，没有新增每条指令判断。最后结束偏移必须包含全部 TB 机器码；以后若增加独立 literal pool 等不属于最后结束偏移的内容，必须同步调整此约定。

## 验证

- 原生对照：基础70、内存加载覆盖RSP/ESP 169、SIMD/MMX故障6，共245个用例通过，包含故障现场和信号返回后重试。
- 隔离诊断版本再次运行公共编码，并逐字节比较内部表与公共表。三个已有程序各512个目标TB，共1536份表全部一致；同时检查该诊断进程执行的其它Quicktrans TB。另两条故障用例通过该诊断版本。
- 关闭 Quicktrans 的 normal-64、normal-zero-middle、normal-single-push 三项测试通过（同时对照 native 与 Quicktrans，共9个输出）。
- 三组性能 proof 的目标仍为完整65条x86指令，内部生成的 LA 代码字节仍为424/416/492。
- 没有专门强制代码区耗尽，没有运行全应用或全指令测试。

## 翻译耗时

同一 Quicktrans 节点对比 LATU 修改前后。沿用512个地址TB、每TB64条目标指令加RET、丢弃前32个、7批交替运行、CPU6。两轮无构建/其它测试干扰。总计时起点及收尾位置不变，第二次编码被省略是本次产品行为的变化。

| 序列 | 第一轮变化 | 第二轮修改前 ns/TB | 第二轮修改后 ns/TB | 第二轮变化 |
|---|---:|---:|---:|---:|
| ADD | -4.03% | 12380 | 11930 | -3.63% |
| PUSH/POP | -6.13% | 12150 | 11420 | -6.01% |
| 混合 | -4.07% | 14600 | 14030 | -3.90% |

不是把分段测量的0.9us直接相减。没有重新测执行耗时或与RSP合并前节点的直接比较，不能混合不同轮次数据重算总增量。该重复编码在RSP合并前也存在，本次优化并非只减少RSP合并自身工作。

## 复现与身份

run-single-search.py / summary-single-search*.json / results-single-search* 保存性能证据。patch-search-verify.py 只用于隔离诊断，不能用于性能测量或提交到产品。search-equality-summary.json 和 search-equality/*.log 保存1536个目标的比较结果。

正确性目录 single-search-base、single-search-load、single-search-vector、single-search-equality-fault、single-search-old-smoke。产品补丁及身份为 single-search.patch 和 single-search-provenance.json。编译程序保存在190的 /home/loongson/latu-rsp-classify-reuse-20261016/single-search-original；观察副本独立保存。

计时/逐字节诊断插桩均已从隔离源码撤下并重建，SHA 与正确性验证副本一致。仅提交本轮 accel 文件及说明，保留原有未提交 latx-config.c 改动；不推送。
