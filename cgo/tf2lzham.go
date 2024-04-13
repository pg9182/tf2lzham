package tf2lzham

// #cgo CFLAGS: -DLZHAM_ANSI_CPLUSPLUS
// #cgo CXXFLAGS: -DLZHAM_ANSI_CPLUSPLUS
// #include "tf2lzham.h"
import "C"

import (
	"errors"
	"unsafe"
)

func Decompress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	if len(dst) == 0 || len(src) == 0 {
		return 0, 0, 0, errors.New("lzham: zero-length buffer")
	}
	var (
		_dst         *C.uint8_t  = (*C.uint8_t)(unsafe.Pointer(&dst[0]))
		_src         *C.uint8_t  = (*C.uint8_t)(unsafe.Pointer(&src[0]))
		_dst_len     *C.size_t   = new(C.size_t)
		_src_len     C.size_t    = C.size_t(len(src))
		_adler32_out *C.uint32_t = (*C.uint32_t)(&adler32)
		_crc32_out   *C.uint32_t = (*C.uint32_t)(&crc32)
	)
	*_dst_len = C.size_t(len(dst))
	if _err := C.tf2lzham_decompress_strerror(C.tf2lzham_decompress(_dst, _dst_len, _src, _src_len, _adler32_out, _crc32_out)); _err != nil {
		return 0, 0, 0, errors.New("lzham: " + C.GoString(_err))
	}
	return int(*_dst_len), adler32, crc32, nil
}

func Compress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	if len(dst) == 0 || len(src) == 0 {
		return 0, 0, 0, errors.New("lzham: zero-length buffer")
	}
	var (
		_dst         *C.uint8_t  = (*C.uint8_t)(unsafe.Pointer(&dst[0]))
		_src         *C.uint8_t  = (*C.uint8_t)(unsafe.Pointer(&src[0]))
		_dst_len     *C.size_t   = new(C.size_t)
		_src_len     C.size_t    = C.size_t(len(src))
		_adler32_out *C.uint32_t = (*C.uint32_t)(&adler32)
		_crc32_out   *C.uint32_t = (*C.uint32_t)(&crc32)
	)
	*_dst_len = C.size_t(len(dst))
	if _err := C.tf2lzham_compress_strerror(C.tf2lzham_compress(_dst, _dst_len, _src, _src_len, _adler32_out, _crc32_out)); _err != nil {
		return 0, 0, 0, errors.New("lzham: " + C.GoString(_err))
	}
	return int(*_dst_len), adler32, crc32, nil
}
