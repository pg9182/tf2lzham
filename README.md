# tf2lzham
Go wrapper for LZHAM with the parameters used for Titanfall 2.

Note that threading and other platform/compiler-specific functionality has been removed.

By default, it uses CGO when enabled, or WebAssembly otherwise. WebAssembly is slower and uses more memory.
