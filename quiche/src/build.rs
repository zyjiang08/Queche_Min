// Additional parameters for Android build of BoringSSL.
//
// Requires Android NDK >= 19.
const CMAKE_PARAMS_ANDROID_NDK: &[(&str, &[(&str, &str)])] = &[
    ("aarch64", &[("ANDROID_ABI", "arm64-v8a")]),
    ("arm", &[("ANDROID_ABI", "armeabi-v7a")]),
    ("x86", &[("ANDROID_ABI", "x86")]),
    ("x86_64", &[("ANDROID_ABI", "x86_64")]),
];

// iOS.
const CMAKE_PARAMS_IOS: &[(&str, &[(&str, &str)])] = &[
    ("aarch64", &[
        ("CMAKE_OSX_ARCHITECTURES", "arm64"),
        ("CMAKE_OSX_SYSROOT", "iphoneos"),
    ]),
    ("x86_64", &[
        ("CMAKE_OSX_ARCHITECTURES", "x86_64"),
        ("CMAKE_OSX_SYSROOT", "iphonesimulator"),
    ]),
];

// ARM Linux.
const CMAKE_PARAMS_ARM_LINUX: &[(&str, &[(&str, &str)])] = &[
    ("aarch64", &[("CMAKE_SYSTEM_PROCESSOR", "aarch64")]),
    ("arm", &[("CMAKE_SYSTEM_PROCESSOR", "arm")]),
];

/// Returns the platform-specific output path for lib.
///
/// MSVC generator on Windows place static libs in a target sub-folder,
/// so adjust library location based on platform and build target.
/// See issue: https://github.com/alexcrichton/cmake-rs/issues/18
fn get_boringssl_platform_output_path() -> String {
    if cfg!(target_env = "msvc") {
        // Code under this branch should match the logic in cmake-rs
        let debug_env_var =
            std::env::var("DEBUG").expect("DEBUG variable not defined in env");

        let deb_info = match &debug_env_var[..] {
            "false" => false,
            "true" => true,
            unknown => panic!("Unknown DEBUG={unknown} env var."),
        };

        let opt_env_var = std::env::var("OPT_LEVEL")
            .expect("OPT_LEVEL variable not defined in env");

        let subdir = match &opt_env_var[..] {
            "0" => "Debug",
            "1" | "2" | "3" =>
                if deb_info {
                    "RelWithDebInfo"
                } else {
                    "Release"
                },
            "s" | "z" => "MinSizeRel",
            unknown => panic!("Unknown OPT_LEVEL={unknown} env var."),
        };

        subdir.to_string()
    } else {
        "".to_string()
    }
}

/// Returns a new cmake::Config for building BoringSSL.
///
/// It will add platform-specific parameters if needed.
fn get_boringssl_cmake_config() -> cmake::Config {
    let arch = std::env::var("CARGO_CFG_TARGET_ARCH").unwrap();
    let os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();
    let pwd = std::env::current_dir().unwrap();

    let mut boringssl_cmake = cmake::Config::new("deps/boringssl");

    // Add platform-specific parameters.
    match os.as_ref() {
        "android" => {
            // We need ANDROID_NDK_HOME to be set properly.
            let android_ndk_home = std::env::var("ANDROID_NDK_HOME")
                .expect("Please set ANDROID_NDK_HOME for Android build");
            let android_ndk_home = std::path::Path::new(&android_ndk_home);
            for (android_arch, params) in CMAKE_PARAMS_ANDROID_NDK {
                if *android_arch == arch {
                    for (name, value) in *params {
                        boringssl_cmake.define(name, value);
                    }
                }
            }
            let toolchain_file =
                android_ndk_home.join("build/cmake/android.toolchain.cmake");
            let toolchain_file = toolchain_file.to_str().unwrap();
            boringssl_cmake.define("CMAKE_TOOLCHAIN_FILE", toolchain_file);

            // 21 is the minimum level tested. You can give higher value.
            boringssl_cmake.define("ANDROID_NATIVE_API_LEVEL", "21");
            boringssl_cmake.define("ANDROID_STL", "c++_shared");

            boringssl_cmake
        },

        "ios" => {
            for (ios_arch, params) in CMAKE_PARAMS_IOS {
                if *ios_arch == arch {
                    for (name, value) in *params {
                        boringssl_cmake.define(name, value);
                    }
                }
            }

            // Bitcode is always on.
            let bitcode_cflag = "-fembed-bitcode";

            // Hack for Xcode 10.1.
            let target_cflag = if arch == "x86_64" {
                "-target x86_64-apple-ios-simulator"
            } else {
                ""
            };

            let cflag = format!("{bitcode_cflag} {target_cflag}");

            boringssl_cmake.define("CMAKE_ASM_FLAGS", &cflag);
            boringssl_cmake.cflag(&cflag);

            boringssl_cmake
        },

        "linux" => match arch.as_ref() {
            "aarch64" | "arm" => {
                for (arm_arch, params) in CMAKE_PARAMS_ARM_LINUX {
                    if *arm_arch == arch {
                        for (name, value) in *params {
                            boringssl_cmake.define(name, value);
                        }
                    }
                }
                boringssl_cmake.define("CMAKE_SYSTEM_NAME", "Linux");
                boringssl_cmake.define("CMAKE_SYSTEM_VERSION", "1");

                boringssl_cmake
            },

            "x86" => {
                boringssl_cmake.define(
                    "CMAKE_TOOLCHAIN_FILE",
                    pwd.join("deps/boringssl/src/util/32-bit-toolchain.cmake")
                        .as_os_str(),
                );

                boringssl_cmake
            },

            _ => boringssl_cmake,
        },

        _ => {
            // Configure BoringSSL for building on 32-bit non-windows platforms.
            if arch == "x86" && os != "windows" {
                boringssl_cmake.define(
                    "CMAKE_TOOLCHAIN_FILE",
                    pwd.join("deps/boringssl/src/util/32-bit-toolchain.cmake")
                        .as_os_str(),
                );
            }

            boringssl_cmake
        },
    }
}

fn write_pkg_config() {
    use std::io::prelude::*;

    let manifest_dir = std::env::var("CARGO_MANIFEST_DIR").unwrap();
    let target_dir = target_dir_path();

    let out_path = target_dir.as_path().join("quiche.pc");
    let mut out_file = std::fs::File::create(out_path).unwrap();

    let include_dir = format!("{manifest_dir}/include");

    let version = std::env::var("CARGO_PKG_VERSION").unwrap();

    let output = format!(
        "# quiche

includedir={include_dir}
libdir={}

Name: quiche
Description: quiche library
URL: https://github.com/cloudflare/quiche
Version: {version}
Libs: -Wl,-rpath,${{libdir}} -L${{libdir}} -lquiche
Cflags: -I${{includedir}}
",
        target_dir.to_str().unwrap(),
    );

    out_file.write_all(output.as_bytes()).unwrap();
}

fn target_dir_path() -> std::path::PathBuf {
    let out_dir = std::env::var("OUT_DIR").unwrap();
    let out_dir = std::path::Path::new(&out_dir);

    for p in out_dir.ancestors() {
        if p.ends_with("build") {
            return p.parent().unwrap().to_path_buf();
        }
    }

    unreachable!();
}

fn main() {
    // Compile chkstk_darwin.c for iOS/macOS targets to fix __chkstk_darwin undefined symbol
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();
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

    if cfg!(feature = "boringssl-vendored") &&
        !cfg!(feature = "boringssl-boring-crate") &&
        !cfg!(feature = "openssl")
    {
        let bssl_dir = std::env::var("QUICHE_BSSL_PATH").unwrap_or_else(|_| {
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

        println!("cargo:rustc-link-arg=-Wl,-rpath,{bssl_dir}");

        let build_path = get_boringssl_platform_output_path();
        let mut build_dir = format!("{bssl_dir}/build/{build_path}");

        // If build directory doesn't exist, use the specified path as is.
        if !std::path::Path::new(&build_dir).is_dir() {
            build_dir = bssl_dir;
        }

        println!("cargo:rustc-link-search=native={build_dir}");

        let bssl_link_kind = std::env::var("QUICHE_BSSL_LINK_KIND")
            .unwrap_or("static".to_string());
        println!("cargo:rustc-link-lib={bssl_link_kind}=ssl");
        println!("cargo:rustc-link-lib={bssl_link_kind}=crypto");
    }

    if cfg!(feature = "boringssl-boring-crate") {
        println!("cargo:rustc-link-lib=static=ssl");
        println!("cargo:rustc-link-lib=static=crypto");
    }

    // Build C++ Engine if feature is enabled
    #[cfg(feature = "cpp-engine")]
    {
        println!("cargo:warning=Building C++ Engine...");
        build_cpp_engine();
        println!("cargo:warning=C++ Engine built successfully");
    }

    // MacOS: Allow cdylib to link with undefined symbols
    let target_os = std::env::var("CARGO_CFG_TARGET_OS").unwrap();
    if target_os == "macos" {
        println!("cargo:rustc-cdylib-link-arg=-Wl,-undefined,dynamic_lookup");
    }

    #[cfg(feature = "openssl")]
    {
        let pkgcfg = pkg_config::Config::new();

        if pkgcfg.probe("libcrypto").is_err() {
            panic!("no libcrypto found");
        }

        if pkgcfg.probe("libssl").is_err() {
            panic!("no libssl found");
        }
    }

    if cfg!(feature = "pkg-config-meta") {
        write_pkg_config();
    }

    #[cfg(feature = "ffi")]
    if target_os != "windows" {
        cdylib_link_lines::metabuild();
    }
}

// ============================================================================
// C++ Engine Build Functions
// ============================================================================

#[cfg(feature = "cpp-engine")]
fn build_cpp_engine() {
    use std::env;

    println!("cargo:rerun-if-changed=engine/src");
    println!("cargo:rerun-if-changed=engine/include");
    println!("cargo:rerun-if-changed=engine/deps/libev");

    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();

    // ============================================================================
    // Build libev from source (vendored)
    // ============================================================================
    println!("cargo:warning=Building vendored libev from source...");

    let mut libev_build = cc::Build::new();
    libev_build
        .file("engine/deps/libev/ev.c")
        .include("engine/deps/libev")
        .warnings(false)  // Suppress warnings from third-party code
        .define("EV_STANDALONE", "1");  // Don't need config.h

    // Platform-specific libev configuration
    match target_os.as_str() {
        "linux" | "android" => {
            libev_build.define("EV_USE_EPOLL", "1");
            libev_build.define("EV_USE_POLL", "1");
            libev_build.define("EV_USE_SELECT", "1");
        }
        "macos" | "ios" => {
            libev_build.define("EV_USE_KQUEUE", "1");
            libev_build.define("EV_USE_POLL", "1");
            libev_build.define("EV_USE_SELECT", "1");
        }
        "freebsd" | "openbsd" | "netbsd" | "dragonfly" => {
            libev_build.define("EV_USE_KQUEUE", "1");
            libev_build.define("EV_USE_POLL", "1");
            libev_build.define("EV_USE_SELECT", "1");
        }
        "windows" => {
            libev_build.define("EV_USE_SELECT", "1");
        }
        _ => {
            libev_build.define("EV_USE_POLL", "1");
            libev_build.define("EV_USE_SELECT", "1");
        }
    }

    libev_build.compile("ev");
    println!("cargo:warning=libev built successfully");

    // ============================================================================
    // Build C++ Engine
    // ============================================================================
    println!("cargo:warning=Building C++ Engine...");

    let mut build = cc::Build::new();
    build
        .cpp(true)
        .flag_if_supported("-std=c++17")
        .warnings(true)
        // Include paths
        .include("engine/include")
        .include("engine/src")
        .include("engine/deps/libev")  // libev headers
        .include("include");            // quiche.h

    // Source files
    build
        .file("engine/src/quiche_engine_api.cpp")
        .file("engine/src/quiche_engine_impl.cpp")
        .file("engine/src/quiche_thread_utils.cpp");

    // Platform-specific configuration
    match target_os.as_str() {
        "macos" | "ios" => {
            if target_os == "ios" {
                let target_arch = env::var("CARGO_CFG_TARGET_ARCH").unwrap();
                let sdk = if target_arch == "x86_64" {
                    "iphonesimulator"
                } else {
                    "iphoneos"
                };

                // Get iOS SDK path by executing xcrun
                let sdk_path = std::process::Command::new("xcrun")
                    .args(&["--sdk", sdk, "--show-sdk-path"])
                    .output()
                    .expect("Failed to execute xcrun");
                let sdk_path_str = String::from_utf8(sdk_path.stdout)
                    .expect("Invalid UTF-8 from xcrun")
                    .trim()
                    .to_string();

                build.flag("-isysroot");
                build.flag(&sdk_path_str);
                build.flag("-fembed-bitcode");
            }
        }
        "android" => {
            let api_level = env::var("ANDROID_API_LEVEL")
                .unwrap_or_else(|_| "21".to_string());
            build.flag(&format!("-D__ANDROID_API__={}", api_level));
            build.flag("-pthread");
        }
        "linux" => {
            build.flag("-pthread");
        }
        "windows" => {
            build.flag("/EHsc");  // Enable C++ exceptions
            build.define("_WIN32_WINNT", "0x0601");  // Windows 7+
        }
        _ => {}
    }

    // Compile C++ Engine (always as static lib intermediate)
    build.compile("quiche_engine");

    // ============================================================================
    // Platform-specific library packaging
    // ============================================================================
    let out_dir = env::var("OUT_DIR").unwrap();
    let out_path = std::path::Path::new(&out_dir);

    // Get paths to compiled libraries
    let libev_path = out_path.join("libev.a");
    let libengine_path = out_path.join("libquiche_engine.a");

    match target_os.as_str() {
        "android" => {
            // Android: Build shared library (libquiche_engine.so)
            println!("cargo:warning=Creating Android shared library (libquiche_engine.so)...");

            let target = env::var("TARGET").unwrap();
            let android_ndk = env::var("ANDROID_NDK_HOME")
                .expect("ANDROID_NDK_HOME must be set for Android build");

            // Determine the NDK toolchain
            let (toolchain_prefix, toolchain_arch) = match target.as_str() {
                t if t.starts_with("aarch64") => ("aarch64-linux-android", "arm64-v8a"),
                t if t.starts_with("armv7") => ("armv7a-linux-androideabi", "armeabi-v7a"),
                t if t.starts_with("i686") => ("i686-linux-android", "x86"),
                t if t.starts_with("x86_64") => ("x86_64-linux-android", "x86_64"),
                _ => panic!("Unsupported Android target: {}", target),
            };

            let api_level = env::var("ANDROID_API_LEVEL").unwrap_or_else(|_| "21".to_string());
            let ndk_path = std::path::Path::new(&android_ndk);
            let toolchain_bin = ndk_path.join(format!(
                "toolchains/llvm/prebuilt/darwin-x86_64/bin/{}{}-clang++",
                toolchain_prefix, api_level
            ));

            // Build shared library command
            let so_output = out_path.join("libquiche_engine.so");
            let link_result = std::process::Command::new(&toolchain_bin)
                .arg("-shared")
                .arg("-o")
                .arg(&so_output)
                .arg("-Wl,--whole-archive")
                .arg(&libengine_path)
                .arg(&libev_path)
                .arg("-Wl,--no-whole-archive")
                .arg("-lc++_shared")
                .arg("-llog")
                .arg("-lm")
                .output();

            match link_result {
                Ok(output) if output.status.success() => {
                    println!("cargo:warning=Android shared library created successfully");
                    println!("cargo:warning=Output: {}", so_output.display());

                    // Copy to a standard location
                    let lib_dir = out_path.parent().unwrap().parent().unwrap().parent().unwrap();
                    let final_so = lib_dir.join("libquiche_engine.so");
                    std::fs::copy(&so_output, &final_so).ok();
                }
                Ok(output) => {
                    println!("cargo:warning=Failed to create shared library");
                    println!("cargo:warning=stdout: {}", String::from_utf8_lossy(&output.stdout));
                    println!("cargo:warning=stderr: {}", String::from_utf8_lossy(&output.stderr));
                }
                Err(e) => {
                    println!("cargo:warning=Failed to execute linker: {}", e);
                }
            }

            // Link standard libraries for Android
            println!("cargo:rustc-link-lib=c++_shared");
            println!("cargo:rustc-link-lib=log");
            println!("cargo:rustc-link-lib=m");
        }

        "ios" => {
            // iOS: Create fat static library (libquiche_engine.a with libev.a included)
            println!("cargo:warning=Creating iOS fat static library (libquiche_engine.a)...");

            // Use libtool to combine archives
            let combined_output = out_path.join("libquiche_engine_fat.a");
            let libtool_result = std::process::Command::new("libtool")
                .arg("-static")
                .arg("-o")
                .arg(&combined_output)
                .arg(&libengine_path)
                .arg(&libev_path)
                .output();

            match libtool_result {
                Ok(output) if output.status.success() => {
                    println!("cargo:warning=iOS fat static library created successfully");
                    println!("cargo:warning=Output: {}", combined_output.display());

                    // Replace the original with combined version
                    std::fs::copy(&combined_output, &libengine_path).ok();

                    // Copy to a standard location
                    let lib_dir = out_path.parent().unwrap().parent().unwrap().parent().unwrap();
                    let final_a = lib_dir.join("libquiche_engine.a");
                    std::fs::copy(&combined_output, &final_a).ok();
                }
                Ok(output) => {
                    println!("cargo:warning=Failed to create fat static library");
                    println!("cargo:warning=stdout: {}", String::from_utf8_lossy(&output.stdout));
                    println!("cargo:warning=stderr: {}", String::from_utf8_lossy(&output.stderr));
                }
                Err(e) => {
                    println!("cargo:warning=Failed to execute libtool: {}", e);
                    println!("cargo:warning=Using ar as fallback...");

                    // Fallback: use ar to combine
                    let ar_result = std::process::Command::new("ar")
                        .arg("-rcs")
                        .arg(&combined_output)
                        .arg(&libengine_path)
                        .arg(&libev_path)
                        .output();

                    if let Ok(output) = ar_result {
                        if output.status.success() {
                            println!("cargo:warning=Combined library created with ar");
                            std::fs::copy(&combined_output, &libengine_path).ok();
                        }
                    }
                }
            }

            // Link standard libraries for iOS
            println!("cargo:rustc-link-lib=c++");
            println!("cargo:rustc-link-lib=framework=Security");
            println!("cargo:rustc-link-lib=framework=Foundation");
        }

        "macos" => {
            // macOS: Keep as separate static libraries
            println!("cargo:rustc-link-lib=c++");
            println!("cargo:rustc-link-lib=framework=Security");
            println!("cargo:rustc-link-lib=framework=Foundation");
        }

        "linux" => {
            // Linux: Keep as separate static libraries
            let target_env = env::var("CARGO_CFG_TARGET_ENV").unwrap_or_default();
            if target_env == "musl" {
                println!("cargo:rustc-link-lib=c++");
            } else {
                println!("cargo:rustc-link-lib=stdc++");
            }
            println!("cargo:rustc-link-lib=pthread");
            println!("cargo:rustc-link-lib=dl");
            println!("cargo:rustc-link-lib=m");
        }

        "windows" => {
            // Windows: Keep as separate static libraries
            println!("cargo:rustc-link-lib=ws2_32");
            println!("cargo:rustc-link-lib=userenv");
        }

        _ => {}
    }

    println!("cargo:warning=C++ Engine built successfully");
}
