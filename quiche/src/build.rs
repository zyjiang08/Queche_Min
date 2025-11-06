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

    println!("cargo:rerun-if-changed=api/src");
    println!("cargo:rerun-if-changed=api/include");

    // Build C++ Engine
    let mut build = cc::Build::new();
    build
        .cpp(true)
        .flag_if_supported("-std=c++17")
        .warnings(true)
        // Include paths
        .include("api/include")
        .include("api/src")
        .include("include");  // quiche.h

    // Source files
    build
        .file("api/src/quiche_engine_api.cpp")
        .file("api/src/quiche_engine_impl.cpp")
        .file("api/src/thread_utils.cpp");

    // Find libev - try pkg-config first, then fallback to manual detection
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap();
    let libev_found = match pkg_config::probe_library("libev") {
        Ok(lib) => {
            // Found via pkg-config
            for path in &lib.include_paths {
                build.include(path);
            }
            for path in &lib.link_paths {
                println!("cargo:rustc-link-search=native={}", path.display());
            }
            for lib_name in &lib.libs {
                println!("cargo:rustc-link-lib={}", lib_name);
            }
            true
        },
        Err(_) => {
            // pkg-config failed, try manual detection
            println!("cargo:warning=libev not found via pkg-config, trying manual detection...");

            // Try common installation paths
            let libev_paths = if target_os == "macos" {
                vec![
                    "/usr/local/opt/libev",     // Homebrew (Intel)
                    "/opt/homebrew/opt/libev",  // Homebrew (Apple Silicon)
                    "/usr/local",                // Manual install
                ]
            } else {
                vec![
                    "/usr",
                    "/usr/local",
                ]
            };

            let mut found = false;
            for base_path in &libev_paths {
                let include_path = format!("{}/include", base_path);
                let lib_path = format!("{}/lib", base_path);
                let header_file = format!("{}/ev.h", include_path);

                if std::path::Path::new(&header_file).exists() {
                    println!("cargo:warning=Found libev at {}", base_path);
                    build.include(&include_path);
                    println!("cargo:rustc-link-search=native={}", lib_path);
                    println!("cargo:rustc-link-lib=ev");
                    found = true;
                    break;
                }
            }

            found
        }
    };

    if !libev_found {
        println!("cargo:warning=libev not found!");
        println!("cargo:warning=Please install libev:");
        println!("cargo:warning=  Ubuntu/Debian: sudo apt-get install libev-dev");
        println!("cargo:warning=  macOS:         brew install libev");
        println!("cargo:warning=  Fedora/RHEL:   sudo yum install libev-devel");
        panic!("libev not found. Please install libev");
    }

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
                build.flag(&format!("-isysroot"));
                build.flag(&format!("$(xcrun --sdk {} --show-sdk-path)", sdk));
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

    // Compile as static library
    build.compile("quiche_engine");

    // Link C++ standard library and platform-specific libraries
    match target_os.as_str() {
        "macos" | "ios" => {
            println!("cargo:rustc-link-lib=c++");
            println!("cargo:rustc-link-lib=framework=Security");
            println!("cargo:rustc-link-lib=framework=Foundation");
        }
        "linux" | "android" => {
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
            println!("cargo:rustc-link-lib=ws2_32");
            println!("cargo:rustc-link-lib=userenv");
        }
        _ => {}
    }
}
