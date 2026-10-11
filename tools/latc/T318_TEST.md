# T-318 测试计划

## 本地回归

- `test-aot-v2-module-pack`：257 地址继续生成两级表；65,537 地址必须生成三级表且
  不删除 TB。
- `test-aot-v2-registry`：验证三级根指针、叶指针、首地址和跨 65,536 边界地址。
- `make -C tools/latc test`：运行格式、模块加载、registry、latcd 和脚本语法测试。

## 3A6000 验收

1. 同步当前源码，删除专用 build/install 目录后 clean build 和 install。
2. 使用固定 Git profile 编译模块，确认 `failed=0`，模块包含全部 profile TB。
3. 使用稳定 cache 离线运行 Git 工作负载，设置
   `LATC_COMPLEX_MIN_GIT_AOT_PERCENT=99.9`，保存 `git-aot-coverage.json`。统计必须
   包含 `module=missing` 的文件型 ELF。
4. 确认暖运行 `compiler_submissions=0`，Git `fsck --strict` 和输出比较通过。
5. 同一构建交替运行 JIT 与暖 AOT，确认暖 AOT 不慢于 JIT。
