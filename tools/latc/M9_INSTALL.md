# AOT v2 M9 构建与用户服务

## 构建和安装

在 LoongArch 3A6000/LASX 主机上配置主 LAT 后，Meson 可直接生成三个运行产物：

```sh
ninja -C build latx-x86_64 liblat-aot-runtime.so.2 latcd
ninja -C build install
```

`latx-x86_64` 安装到 `bindir`，`latcd` 安装到 `bindir`，
`liblat-aot-runtime.so.2` 安装到 `libdir`。构建目录中的 runner 带有 `$ORIGIN`
RPATH，可直接加载同目录的运行库。安装后应由系统动态链接器在 `libdir` 找到它。

`latc` 编译器仍由 `make -C tools/latc` 构建。这是当前版本的明确限制；主 LAT
不再需要 staging 源码覆盖来构建 runner，但 latcd 仍需一个 `latc` 可执行文件来
生成模块。

## 每用户 latcd

以下示例使用每个用户独立的 socket 和缓存。`ROOTFS` 必须是 x86-64 rootfs，
`LATC`、`RUNNER` 和 `RUNTIME_DIR` 必须使用实际安装路径。

```sh
cache=${XDG_CACHE_HOME:-$HOME/.cache}/latx/aot-v2
socket=${XDG_RUNTIME_DIR:-/tmp}/latcd-$UID.sock
mkdir -m 700 -p "$cache"

latcd --serve \
  --socket "$socket" \
  --cache-dir "$cache" \
  --latc "$LATC" \
  --runner "$RUNNER" \
  --runtime-dir "$RUNTIME_DIR" \
  --x86-rootfs "$ROOTFS" \
  --workers 2 &

export LATX_AOT_V2_CACHE_DIR=$cache
export LATX_AOT_V2_LATCD_SOCKET=$socket
```

latcd 不可用、请求失败或缓存文件无效时，runner 会继续使用 JIT。应用不应依赖
latcd 已经完成编译。退出用户会话时，应先向 latcd 发送 `SIGTERM` 并等待退出，
避免留下编译子进程。
