package tf2zham

import (
	"bytes"
	"context"
	_ "embed"
	"errors"
	"fmt"
	"sync"

	"github.com/tetratelabs/wazero"
	"github.com/tetratelabs/wazero/imports/wasi_snapshot_preview1"
)

//go:generate sh -c "docker run --rm -i -v \"$(pwd)/..:/src\" -w /src -u $(id -u):$(id -g) ghcr.io/webassembly/wasi-sdk:wasi-sdk-21 sh -euxc '${DOLLAR}CXX ${DOLLAR}CXXFLAGS -g -nostartfiles -std=c++11 -DLZHAM_ANSI_CPLUSPLUS -DNDEBUG -Oz -Wall -Wno-unused-value -fno-exceptions -ffast-math cgo/*.cpp -Wl,--no-entry -Wl,--export-dynamic -o wasm/tf2lzham.wasm'"
//go:embed tf2lzham.wasm
var wasm []byte

var (
	compile sync.Once
	runtime wazero.Runtime
	module  wazero.CompiledModule
)

func EnsureCompiled() {
	compile.Do(func() {
		ctx := context.Background()

		runtime = wazero.NewRuntime(ctx)

		_, err := wasi_snapshot_preview1.Instantiate(ctx, runtime)
		if err != nil {
			panic(fmt.Errorf("tf2lzham/wasm: failed to instantiate wasi runtime: %w", err))
		}

		module, err = runtime.CompileModule(ctx, wasm)
		if err != nil {
			panic(fmt.Errorf("tf2lzham/wasm: failed to compile module: %w", err))
		}
	})
}

func execute(dst, src []byte, decompress bool) (int, uint32, uint32, error) {
	if len(dst) == 0 || len(src) == 0 {
		return 0, 0, 0, errors.New("lzham: zero-length buffer")
	}
	ctx := context.Background()

	var method string
	if decompress {
		method = "decompress"
	} else {
		method = "compress"
	}

	EnsureCompiled()

	// we cannot use an instantiated module concurrently
	// - we grow the instantiated memory to fit the buffer
	// - if we use a pool, we may leak memory
	// - instantiation is fast enough (a few hundred microseconds)
	// - so instantiate it for every call
	instance, err := runtime.InstantiateModule(ctx, module, wazero.NewModuleConfig().WithName(""))
	if err != nil {
		return 0, 0, 0, err
	}
	defer instance.Close(ctx)

	var (
		malloc   = instance.ExportedFunction("tf2lzham_malloc")
		compress = instance.ExportedFunction("tf2lzham_" + method)
		strerror = instance.ExportedFunction("tf2lzham_" + method + "_strerror")
	)
	if malloc == nil || compress == nil || strerror == nil {
		return 0, 0, 0, fmt.Errorf("wasm: missing expected symbol")
	}

	mem := instance.Memory()

	var (
		lenLen = uint32(32 / 4)
		adlLen = uint32(32 / 4)
		crcLen = uint32(32 / 4)
		dstLen = uint32(len(dst))
		srcLen = uint32(len(src))
	)

	var ptr uint32
	if r, err := malloc.Call(ctx, uint64(lenLen+adlLen+crcLen+dstLen+srcLen)); err != nil {
		return 0, 0, 0, err
	} else {
		ptr = uint32(r[0])
	}

	var (
		lenOff = ptr
		adlOff = lenOff + lenLen
		crcOff = adlOff + adlLen
		dstOff = crcOff + crcLen
		srcOff = dstOff + dstLen
	)
	mem.WriteUint32Le(lenOff, dstLen)
	mem.WriteUint32Le(adlOff, 0)
	mem.WriteUint32Le(crcOff, 0)
	mem.Write(srcOff, src)

	if r, err := compress.Call(ctx, uint64(dstOff), uint64(lenOff), uint64(srcOff), uint64(srcLen), uint64(adlOff), uint64(crcOff)); err != nil {
		return 0, 0, 0, err
	} else if r, err := strerror.Call(ctx, r[0]); err != nil {
		return 0, 0, 0, err
	} else if ptr := uint32(r[0]); ptr != 0 {
		b, _ := instance.Memory().Read(ptr, 128)
		if n := bytes.IndexByte(b, 0); n != -1 {
			b = b[:n]
		} else {
			return 0, 0, 0, fmt.Errorf("wasm: strerror returned an invalid string")
		}
		return 0, 0, 0, errors.New(string(b))
	}

	var (
		adler32, _ = mem.ReadUint32Le(adlOff)
		crc32, _   = mem.ReadUint32Le(crcOff)
		lenVal, _  = mem.ReadUint32Le(lenOff)
		dstMem, _  = mem.Read(dstOff, lenVal)
	)
	copy(dst, dstMem)

	return int(lenVal), adler32, crc32, nil
}

func Decompress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	return execute(dst, src, true)
}

func Compress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	return execute(dst, src, false)
}
