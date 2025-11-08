# libquiche_engine Size Optimization Plan

## Current Snapshot
- **Android**: `libquiche_engine.so` 已可从 ~12 MB（含调试符号）通过 `strip` 压到 ~2.1 MB，但默认构建始终启用 `boringssl-vendored` + `http3`，且链接命令使用 `--whole-archive`，导致进一步裁剪空间受限。
- **iOS**: `libquiche_engine.a` 由 `libtool -static` 合并 `libquiche.a`、libev、C++ 引擎与 BoringSSL，缺少按需 feature 组合以及导出符号控制。
- **Rust 配置**: 根 `Cargo.toml` 使用 `opt-level = "z"`、`codegen-units = 1`、`panic = "abort"`，但 mobile 构建脚本未传入 `--no-default-features`，无法关闭 `http3`/`qlog` 等可选模块。

## Feature & Dependency Tuning
1. **HTTP/3 可选化**  
   - 在 `quiche_engine_all.sh` 中添加参数开关，默认使用 `--no-default-features --features ffi,cpp-engine,boringssl-vendored`，仅在需要 HTTP/3 时显式追加。预计再减 30–40% 代码段。
2. **平台特定 feature 组合**  
   - 为 Android/iOS 建立 `ANDROID_FEATURES`/`IOS_FEATURES` 环境变量，允许裁剪 `qlog`、`sfv`、`gcongestion` 等模块，并在 `docs/mobile/` 标注不同 SDK 变体用途。
3. **算法白名单**  
   - 根据实际握手配置在 `quiche/src/build.rs` 中追加 `OPENSSL_NO_ARIA`, `OPENSSL_NO_CHACHA20_POLY1305`, `OPENSSL_NO_EC2M` 等宏，只保留 `TLS_AES_128_GCM_SHA256`、`TLS_CHACHA20_POLY1305_SHA256` 与 P-256 曲线，减少 BoringSSL 代码与常量。

## BoringSSL Specialization
- **默认启用 `QUICHE_MINIMAL_BSSL`**：在 release 构建阶段 `export QUICHE_MINIMAL_BSSL=1`，触发 build.rs 关闭错误字符串与 stdio 依赖，可减 5–10% 常量区。
- **`OPENSSL_SMALL`**：在 `get_boringssl_cmake_config()` 里追加 `boringssl_cmake.define("OPENSSL_SMALL","1")`，让 BoringSSL 使用压缩查找表实现；需回归性能。
- **按需 strip**：构建完成后保留 `.full` 版本，并对分发的 `.so/.a` 执行 `llvm-strip --strip-debug`（Android）或 `strip -S`（iOS），同时存储 `.dbg` 以便符号化。

## Build & Link Optimizations
1. **Rust 链接器 GC**  
   - 在 `.cargo/config` 或脚本中注入 `RUSTFLAGS="-C link-arg=-Wl,--gc-sections -C link-arg=-Wl,-O2"`，配合 `-ffunction-sections/-fdata-sections`，让 `libquiche.a` 未引用模块被剔除。
2. **版本脚本/导出列表**  
   - 为 Android 生成 `tools/mobile/exported_symbols.map`（列出 `QuicheEngine*` 与必要 FFI 函数），在链接命令传入 `-Wl,--version-script=...`，即可移除 `--whole-archive`；iOS 使用 `-exported_symbols_list` 达到同样效果。
3. **模块拆分**  
   - 长期可考虑将 `quiche` 拆分为 `quiche-core`（传输+TLS）与 `quiche-h3`/`quiche-qlog` 等 crate，移动端只静态链接核心库以匹配需求。

## C/C++ Engine Slimming
- **编译标志**：在 `build_cpp_engine()` 中为 libev 与 C++ 源启用 `-Oz -fvisibility=hidden -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti`（iOS 仍需 `-fembed-bitcode`），并设置 `.opt_level(2)` 以减少默认 O0 体积。
- **日志裁剪**：用宏包裹 `engine/src` 中的大量 `std::cout/ std::cerr`，默认关闭或重定向到外部回调，避免格式化字符串常驻。
- **可选模块**：将调试/统计 API 单独打成 `libquiche_engine_debug.a`，常规 SDK 仅包含核心 API，按需合并。

## Packaging & Reporting
- **自动 size 报告**：在 `quiche_engine_all.sh` 末尾运行 `llvm-size`, `nm --size-sort`, `bloaty`（若可用），并写入 `docs/size-analysis/latest-<platform>.md`，持续跟踪优化效果。
- **多档产物**：Android 输出 `.full` 与 `.stripped` 两个版本；iOS 提供 `libquiche_engine_min.a`（核心 API）与 `libquiche_engine_full.a`（含调试功能），方便按需取用。
- **质量门槛**：在 PR 模板新增 “Size delta” 字段，要求提交者记录 `du -h libs/<platform>/libquiche_engine*` 前后差异，并确认 `cargo fmt`, `cargo clippy`, `cargo test`, `quiche_engine_all.sh` 对应目标已执行。

## Next Steps
1. **短期**  
   - 更新 `quiche_engine_all.sh` 以支持 `--no-default-features`、`QUICHE_MINIMAL_BSSL` 与自动 strip。
   - 引入导出符号列表，移除 Android `--whole-archive` 并观察 `.so` 体积变化。
2. **中期**  
   - 根据产品需求默认禁用 HTTP/3，评估 BoringSSL 进一步算法裁剪；为 C++ 引擎追加精简编译标志与按需日志。
3. **长期**  
   - 模块化 `quiche` crate 结构，引入 PGO/LTO once 优化，以及提供多档 SDK 以满足不同集成场景。
