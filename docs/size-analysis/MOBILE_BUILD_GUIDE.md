# Mobile Build & Size Automation

## 快速开始
1. 设置 NDK 路径：`export ANDROID_NDK_HOME=/opt/android/android-ndk-r21e`
2. 执行脚本：`./quiche_engine_all.sh android arm64-v8a`
3. 产物：
   - `lib/android/<abi>/libquiche_engine.so`（默认 strip，调试符号保存在 `.dbg`）
   - `lib/android/<abi>/libquiche_engine.a`
   - `include/` 内的头文件
   - `docs/size-analysis/latest-<platform>-<arch>.md` 尺寸报告

## 特性选择
脚本通过以下环境变量控制 `cargo` features：

| 变量 | 说明 | 默认值 |
| --- | --- | --- |
| `MOBILE_FEATURES` | 所有平台通用 features | `boringssl-vendored,ffi,cpp-engine,qlog` |
| `MOBILE_ANDROID_FEATURES` | 覆盖 Android | 同 `MOBILE_FEATURES` |
| `MOBILE_IOS_FEATURES` | 覆盖 iOS | 同上 |
| `MOBILE_MACOS_FEATURES` | 覆盖 macOS | 同上 |

示例：启用 HTTP/3
```bash
export MOBILE_ANDROID_FEATURES="boringssl-vendored,ffi,cpp-engine,qlog,http3"
./quiche_engine_all.sh android arm64-v8a
```

## 体积自动化
- **strip**：构建完成后自动调用 `llvm-strip`（若可用）移除未用符号，并在同目录输出 `.dbg` 备份。
- **尺寸报告**：脚本将 `du -h`、`size/llvm-size` 结果写入 `docs/size-analysis/latest-<platform>-<arch>.md`，便于追踪波动。
- **BoringSSL 裁剪**：默认导出 `QUICHE_MINIMAL_BSSL=1`，移除错误字符串/stdio 依赖。

## 产物精简
- **版本脚本**：`tools/mobile/exported_symbols.map` 仅保留 `QuicheEngine*` 与 `quiche_*` 符号，减少 `.dynsym/.dynstr`。
- **一次性合并**：Android 只对 `libquiche.a` 执行 `--whole-archive`，避免重复链接 libev/BoringSSL 带来的体积膨胀。

## 定制输出目录
通过 `MOBILE_CARGO_TARGET_DIR=/path/to/target` 可将 Rust 目标/临时文件放到单独挂载点；脚本会自动配置 `TMPDIR/CARGO_TARGET_TMPDIR/RUSTC_TMPDIR` 避免跨设备 `rename`。
