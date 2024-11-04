# Template for a custom hunter configuration Useful when there is a need for a
# non-default version or arguments of a dependency, or when a project not
# registered in soramitsu-hunter should be added.
#
# hunter_config(
#     package-name
#     VERSION 0.0.0-package-version
#     CMAKE_ARGS
#      CMAKE_VARIABLE=value
# )
#
# hunter_config(
#     package-name
#     URL https://repo/archive.zip
#     SHA1 1234567890abcdef1234567890abcdef12345678
#     CMAKE_ARGS
#       CMAKE_VARIABLE=value
# )

hunter_config(
    backward-cpp
    URL https://github.com/bombela/backward-cpp/archive/refs/tags/v1.6.zip
    SHA1 93c4c843fc9308e62ac462459077d87dc6dd9885
    CMAKE_ARGS BACKWARD_TESTS=OFF
    KEEP_PACKAGE_SOURCES
)

hunter_config(
    rocksdb
    VERSION 9.6.1
    CMAKE_ARGS WITH_GFLAGS=OFF USE_RTTI=ON
)

hunter_config(
    BoringSSL
    URL  https://github.com/qdrvm/boringssl/archive/bc72a3fa91d52d6c1db45c24efd863d180c7d98c.zip
    SHA1 64b281459365194c7d1f561b075c25138e122926
)

if ("${WASM_COMPILER}" STREQUAL "WasmEdge")
  hunter_config(
      fmt
      URL
          https://github.com/fmtlib/fmt/archive/refs/tags/10.2.1.tar.gz
      SHA1
          d223964b782d2562d6722ffe67027204c6035453
      CMAKE_ARGS
          CMAKE_POSITION_INDEPENDENT_CODE=TRUE
  )

  hunter_config(
      spdlog
      VERSION 1.12.0-p0
      CMAKE_ARGS
          SPDLOG_BUILD_PIC=ON
          SPDLOG_FMT_EXTERNAL=ON
  )

  hunter_config(
      WasmEdge
      URL https://github.com/qdrvm/WasmEdge/archive/refs/tags/0.14.1.zip
      SHA1 ${WASMEDGE_ID}
      CMAKE_ARGS
        WASMEDGE_BUILD_STATIC_LIB=ON
        WASMEDGE_BUILD_SHARED_LIB=OFF
        CMAKE_CXX_FLAGS=-Wno-error=maybe-uninitialized
      KEEP_PACKAGE_SOURCES
  )
endif ()

hunter_config(
      WasmEdge
      URL  https://github.com/qdrvm/WasmEdge/archive/refs/heads/fix/libdir.zip
      SHA1 0bd0d0d5dc3e377c65a17f2fe309c5cdb9668755
      CMAKE_ARGS
        WASMEDGE_BUILD_STATIC_LIB=ON
        WASMEDGE_BUILD_SHARED_LIB=OFF
      KEEP_PACKAGE_SOURCES
)

if ("${WASM_COMPILER}" STREQUAL "WAVM")
  hunter_config(
      LLVM
      VERSION 12.0.1-p4
      CMAKE_ARGS
        LLVM_ENABLE_PROJECTS=ir
        KEEP_PACKAGE_SOURCES
  )

  if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    set(WAVM_CXX_FLAGS -Wno-redundant-move;-Wno-dangling-reference;-Wno-error=extra;)
  else ()
    set(WAVM_CXX_FLAGS -Wno-redundant-move)
  endif ()

  hunter_config(
      wavm
      VERSION 1.0.14
      CMAKE_ARGS
        WAVM_CXX_FLAGS=${WAVM_CXX_FLAGS}
      KEEP_PACKAGE_SOURCES
  )
endif ()

hunter_config(
    kagome-crates
    URL  https://github.com/qdrvm/kagome-crates/archive/refs/tags/v1.0.3.tar.gz
    SHA1 4207446a0e45764b814805821aa6860924b03cb7
)

hunter_config(
    libsecp256k1
    VERSION 0.5.1
    CMAKE_ARGS
      SECP256K1_ENABLE_MODULE_RECOVERY=ON
)

hunter_config(
    erasure_coding_crust
    VERSION 0.0.9
    KEEP_PACKAGE_SOURCES
)

hunter_config(
    soralog
    VERSION 0.2.4
    KEEP_PACKAGE_SOURCES
)
