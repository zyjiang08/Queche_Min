// 这是一个示例 build.rs 实现，展示如何在 Cargo 构建中编译 C++ Engine
// 路径: quiche/src/build.rs

use std::env;
use std::path::PathBuf;

fn main() {
    // ============================================================================
    // 现有逻辑：构建 BoringSSL
    // ============================================================================
    if cfg!(feature = "boringssl-vendored") &&
        !cfg!(feature = "boringssl-boring-crate") &&
        !cfg!(feature = "openssl")
    {
        build_boringssl();
    }

    // ============================================================================
    // ⭐ 新增逻辑：构建 C++ Engine
    // ============================================================================
    #[cfg(feature = "cpp-engine")]
    {
        println!("cargo:warning=Building C++ Engine...");
        build_cpp_engine();
        println!("cargo:warning=C++ Engine built successfully");
    }

    // ============================================================================
    // 现有逻辑：其他配置
    // ============================================================================
    configure_platform_specific();

    #[cfg(feature = "pkg-config-meta")]
    write_pkg_config();

    #[cfg(feature = "ffi")]
    if env::var("CARGO_CFG_TARGET_OS").unwrap() != "windows" {
        cdylib_link_lines::metabuild();
    }
}

// ============================================================================
// 现有函数：构建 BoringSSL（保持不变）
// ============================================================================
fn build_boringssl() {
    let bssl_dir = env::var("QUICHE_BSSL_PATH").unwrap_or_else(|_| {
        let mut cfg = get_boringssl_cmake_config();

        if cfg!(feature = "fuzzing") {
            cfg.cxxflag("-DBORINGSSL_UNSAFE_DETERMINISTIC_MODE")
                .cxxflag("-DBORINGSSL_UNSAFE_FUZZER_MODE");
            cfg.cflag("-DBORINGSSL_UNSAFE_DETERMINISTIC_MODE")
                .cflag("-DBORINGSSL_UNSAFE_FUZZER_MODE");
        }

        cfg.build_target("ssl").build();
        cfg.build_target("crypto").build().display().to_string()
    });

    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", bssl_dir);

    let build_path = get_boringssl_platform_output_path();
    let mut build_dir = format!("{}/build/{}", bssl_dir, build_path);

    if !std::path::Path::new(&build_dir).is_dir() {
        build_dir = bssl_dir;
    }

    println!("cargo:rustc-link-search=native={}", build_dir);

    let bssl_link_kind = env::var("QUICHE_BSSL_LINK_KIND")
        .unwrap_or_else(|_| "static".to_string());
    println!("cargo:rustc-link-lib={}=ssl", bssl_link_kind);
    println!("cargo:rustc-link-lib={}=crypto", bssl_link_kind);
}

fn get_boringssl_cmake_config() -> cmake::Config {
    // 现有实现...
    cmake::Config::new("deps/boringssl")
}

fn get_boringssl_platform_output_path() -> String {
    // 现有实现...
    "".to_string()
}

// ============================================================================
// ⭐ 新增函数：构建 C++ Engine
// ============================================================================

#[cfg(feature = "cpp-engine")]
fn build_cpp_engine() {
    // 检查 C++ 编译器
    check_cpp_compiler_version();

    // 查找或构建 libev
    let libev_paths = find_or_build_libev();

    // 编译 C++ Engine
    compile_cpp_engine(&libev_paths);

    // 配置链接
    link_cpp_engine(&libev_paths);
}

#[cfg(feature = "cpp-engine")]
fn check_cpp_compiler_version() {
    println!("cargo:rerun-if-changed=api/src");
    println!("cargo:rerun-if-changed=api/include");

    let compiler = cc::Build::new()
        .cpp(true)
        .flag("-std=c++17")
        .get_compiler();

    println!("cargo:warning=Using C++ compiler: {:?}", compiler.path());

    // 检查编译器是否支持 C++17
    let test_code = r#"
        #include <variant>
        #include <string>
        int main() {
            std::variant<int, std::string> v = 42;
            return 0;
        }
    "#;

    let mut test_build = cc::Build::new();
    test_build
        .cpp(true)
        .flag("-std=c++17");

    // 尝试编译测试代码
    match test_build.try_compile("cpp17_test") {
        Ok(_) => println!("cargo:warning=C++17 support: OK"),
        Err(e) => {
            println!("cargo:warning=C++17 support check failed: {}", e);
            println!("cargo:warning=Please install a C++17-compatible compiler:");
            println!("cargo:warning=  - GCC 7+ or Clang 5+");
            panic!("C++17 compiler required for cpp-engine feature");
        }
    }
}

#[cfg(feature = "cpp-engine")]
struct LibevPaths {
    include_paths: Vec<PathBuf>,
    lib_paths: Vec<PathBuf>,
    libs: Vec<String>,
}

#[cfg(feature = "cpp-engine")]
fn find_or_build_libev() -> LibevPaths {
    // 尝试使用 pkg-config 查找系统 libev
    if let Ok(libev) = pkg_config::probe_library("libev") {
        println!("cargo:warning=Found system libev via pkg-config");
        return LibevPaths {
            include_paths: libev.include_paths,
            lib_paths: libev.link_paths,
            libs: libev.libs,
        };
    }

    // 如果启用了 vendored 特性，构建 vendored libev
    #[cfg(feature = "cpp-engine-vendored")]
    {
        println!("cargo:warning=Building vendored libev...");
        return build_vendored_libev();
    }

    // 尝试在常见路径查找
    if let Some(paths) = find_libev_in_common_paths() {
        println!("cargo:warning=Found libev in common paths");
        return paths;
    }

    // 如果都失败，给出错误信息
    println!("cargo:warning=libev not found!");
    println!("cargo:warning=Please install libev:");
    println!("cargo:warning=  Ubuntu/Debian: sudo apt-get install libev-dev");
    println!("cargo:warning=  macOS:         brew install libev");
    println!("cargo:warning=  Fedora/RHEL:   sudo yum install libev-devel");
    println!("cargo:warning=  Windows:       vcpkg install libev");
    println!("cargo:warning=");
    println!("cargo:warning=Or enable vendored libev:");
    println!("cargo:warning=  cargo build --features cpp-engine-vendored");
    panic!("libev not found and vendored feature not enabled");
}

#[cfg(feature = "cpp-engine")]
fn find_libev_in_common_paths() -> Option<LibevPaths> {
    let common_paths = vec![
        "/usr/local",
        "/usr",
        "/opt/local",
        "/opt/homebrew",  // macOS ARM
    ];

    for prefix in common_paths {
        let include_path = PathBuf::from(format!("{}/include", prefix));
        let lib_path = PathBuf::from(format!("{}/lib", prefix));
        let ev_h = include_path.join("ev.h");

        if ev_h.exists() {
            // 检查库文件
            for lib_name in &["libev.a", "libev.dylib", "libev.so"] {
                if lib_path.join(lib_name).exists() {
                    println!("cargo:warning=Found libev at {}", prefix);
                    return Some(LibevPaths {
                        include_paths: vec![include_path],
                        lib_paths: vec![lib_path],
                        libs: vec!["ev".to_string()],
                    });
                }
            }
        }
    }

    None
}

#[cfg(all(feature = "cpp-engine", feature = "cpp-engine-vendored"))]
fn build_vendored_libev() -> LibevPaths {
    use cmake::Config;

    // 假设我们在 deps/libev 有 libev 源码
    let dst = Config::new("deps/libev")
        .define("CMAKE_BUILD_TYPE", "Release")
        .define("BUILD_SHARED_LIBS", "OFF")
        .build();

    let include_path = dst.join("include");
    let lib_path = dst.join("lib");

    println!("cargo:warning=Built vendored libev at {:?}", dst);

    LibevPaths {
        include_paths: vec![include_path],
        lib_paths: vec![lib_path],
        libs: vec!["ev".to_string()],
    }
}

#[cfg(feature = "cpp-engine")]
fn compile_cpp_engine(libev: &LibevPaths) {
    let mut build = cc::Build::new();

    build
        .cpp(true)
        .flag("-std=c++17")
        .warnings(true)
        .extra_warnings(true)
        // 包含路径
        .include("api/include")
        .include("api/src")
        .include("include")  // quiche.h
        // 源文件
        .file("api/src/quiche_engine_api.cpp")
        .file("api/src/quiche_engine_impl.cpp")
        .file("api/src/thread_utils.cpp");

    // 添加 libev 包含路径
    for path in &libev.include_paths {
        build.include(path);
    }

    // 平台特定编译标志
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();

    match target_os.as_str() {
        "macos" | "ios" => configure_apple(&mut build, &target_os, &target_arch),
        "linux" | "android" => configure_linux_android(&mut build, &target_os),
        "windows" => configure_windows(&mut build),
        _ => {}
    }

    // 编译为静态库
    build.compile("quiche_engine");

    println!("cargo:warning=Compiled C++ Engine successfully");
}

#[cfg(feature = "cpp-engine")]
fn configure_apple(build: &mut cc::Build, os: &str, arch: &str) {
    if os == "ios" {
        // iOS 特定配置
        let sdk = if arch == "x86_64" {
            "iphonesimulator"
        } else {
            "iphoneos"
        };
        build.flag(&format!("-isysroot $(xcrun --sdk {} --show-sdk-path)", sdk));
        build.flag("-fembed-bitcode");
    }

    // macOS/iOS 通用配置
    build.flag("-framework").flag("Security");
    build.flag("-framework").flag("Foundation");
}

#[cfg(feature = "cpp-engine")]
fn configure_linux_android(build: &mut cc::Build, os: &str) {
    if os == "android" {
        // Android NDK 配置
        let ndk_home = env::var("ANDROID_NDK_HOME")
            .or_else(|_| env::var("NDK_HOME"))
            .expect("ANDROID_NDK_HOME or NDK_HOME must be set for Android builds");

        println!("cargo:warning=Using Android NDK: {}", ndk_home);

        // Android API level
        let api_level = env::var("ANDROID_API_LEVEL")
            .unwrap_or_else(|_| "21".to_string());

        build.flag(&format!("-D__ANDROID_API__={}", api_level));
    }

    // Linux 特定标志
    build.flag("-pthread");
}

#[cfg(feature = "cpp-engine")]
fn configure_windows(build: &mut cc::Build) {
    // Windows 特定配置
    build.flag("/std:c++17");
    build.flag("/EHsc");  // 启用 C++ 异常
    build.define("_WIN32_WINNT", "0x0601");  // Windows 7+
}

#[cfg(feature = "cpp-engine")]
fn link_cpp_engine(libev: &LibevPaths) {
    // 链接 libev
    for path in &libev.lib_paths {
        println!("cargo:rustc-link-search=native={}", path.display());
    }
    for lib in &libev.libs {
        println!("cargo:rustc-link-lib={}", lib);
    }

    // 链接 C++ 标准库
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();

    match target_os.as_str() {
        "macos" | "ios" => {
            // macOS/iOS 使用 libc++
            println!("cargo:rustc-link-lib=c++");
            println!("cargo:rustc-link-lib=framework=Security");
            println!("cargo:rustc-link-lib=framework=Foundation");
        }
        "linux" | "android" => {
            // Linux/Android 可能使用 libstdc++ 或 libc++
            if target_env == "musl" {
                // musl 通常使用 libc++
                println!("cargo:rustc-link-lib=c++");
            } else {
                // glibc 通常使用 libstdc++
                println!("cargo:rustc-link-lib=stdc++");
            }
            println!("cargo:rustc-link-lib=pthread");
            println!("cargo:rustc-link-lib=dl");
            println!("cargo:rustc-link-lib=m");
        }
        "windows" => {
            // Windows MSVC 自动链接 C++ 标准库
            println!("cargo:rustc-link-lib=ws2_32");
            println!("cargo:rustc-link-lib=userenv");
        }
        _ => {}
    }

    println!("cargo:warning=Configured C++ Engine linking");
}

// ============================================================================
// 现有函数：平台特定配置（保持不变）
// ============================================================================
fn configure_platform_specific() {
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();

    // macOS: 允许 cdylib 链接未定义符号
    if target_os == "macos" {
        println!("cargo:rustc-cdylib-link-arg=-Wl,-undefined,dynamic_lookup");
    }

    // iOS: 编译 chkstk_darwin.c
    if target_os == "ios" {
        println!("cargo:warning=Compiling chkstk_darwin.c for iOS target");
        println!("cargo:rerun-if-changed=chkstk_darwin.c");

        cc::Build::new()
            .file("chkstk_darwin.c")
            .opt_level(2)
            .warnings(false)
            .compile("chkstk_darwin");

        println!("cargo:warning=Successfully compiled chkstk_darwin support");
    }
}

fn write_pkg_config() {
    // 现有实现...
}
