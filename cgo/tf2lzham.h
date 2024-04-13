#include <stddef.h>
#include <stdint.h>

#define TF2LZHAM_EXPORT __attribute__((visibility("default")))

#ifdef __cplusplus
extern "C" {
#endif

TF2LZHAM_EXPORT void *tf2lzham_malloc(size_t sz);
TF2LZHAM_EXPORT uint32_t tf2lzham_compress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, uint32_t *adler32_out, uint32_t *crc32_out);
TF2LZHAM_EXPORT uint32_t tf2lzham_decompress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, uint32_t *adler32_out, uint32_t *crc32_out);
TF2LZHAM_EXPORT const char *tf2lzham_compress_strerror(uint32_t status);
TF2LZHAM_EXPORT const char *tf2lzham_decompress_strerror(uint32_t status);

#ifdef __cplusplus
}
#endif
