//go:build cgo && !tf2lzhamgo

package tf2lzham

import tf2lzham "github.com/pg9182/tf2lzham/cgo"

const WebAssembly = false

func Decompress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	return tf2lzham.Decompress(dst, src)
}

func Compress(dst, src []byte) (n int, adler32, crc32 uint32, err error) {
	return tf2lzham.Compress(dst, src)
}
