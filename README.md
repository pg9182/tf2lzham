# tf2lzham
Go wrapper for LZHAM with the parameters used for Titanfall 2.

Note that threading and other platform/compiler-specific functionality has been removed.

It should also be possible to use this under WebAssembly with a command like `/opt/wasi-sdk-14.0/bin/clang++ -s -nostartfiles --sysroot=/opt/wasi-sdk-14.0/share/wasi-sysroot -DLZHAM_ANSI_CPLUSPLUS -DNDEBUG -Oz -Wall -Wno-unused-value -fno-exceptions -ffast-math *.cpp -Wl,--no-entry -Wl,--export-dynamic -o tf2lzham.wasm`.
