//go:build !cgo
// +build !cgo

package tf2lzham

import "errors"

func Decompress(dst, src []byte) (adler32, crc32 uint32, err error) {
	return 0, 0, errors.New("lzham: cgo required")
}

func Compress(dst, src []byte) (adler32, crc32 uint32, err error) {
	return 0, 0, errors.New("lzham: cgo required")
}
