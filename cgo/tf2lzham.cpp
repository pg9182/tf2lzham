#include "tf2lzham.h"
#include "lzham.h"

static const lzham_uint32 tf2lzham_dict_size = 20; // required for compatibility

static const lzham_compress_params tf2lzham_compress_params = {
    .m_struct_size = sizeof(lzham_compress_params),
    .m_dict_size_log2 = tf2lzham_dict_size,
    .m_level = LZHAM_COMP_LEVEL_UBER,
    .m_compress_flags = LZHAM_COMP_FLAG_DETERMINISTIC_PARSING,
};

static const lzham_decompress_params tf2lzham_decompress_params = {
    .m_struct_size = sizeof(lzham_decompress_params),
    .m_dict_size_log2 = 20,
    .m_decompress_flags = LZHAM_DECOMP_FLAG_OUTPUT_UNBUFFERED | LZHAM_DECOMP_FLAG_COMPUTE_ADLER32 | LZHAM_DECOMP_FLAG_COMPUTE_CRC32,
};

#ifdef __wasm__
static_assert(sizeof(size_t) == sizeof(uint32_t), "expected size_t to be uint32 for WebAssembly");
#endif

extern "C" void *tf2lzham_malloc(size_t sz) {
    return operator new(sz);
}

extern "C" uint32_t tf2lzham_compress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, uint32_t *adler32_out, uint32_t *crc32_out) {
    return lzham_compress_memory(&tf2lzham_compress_params, dst, dst_len, src, src_len, adler32_out, crc32_out);
}

extern "C" uint32_t tf2lzham_decompress(uint8_t *dst, size_t *dst_len, const uint8_t *src, size_t src_len, uint32_t *adler32_out, uint32_t *crc32_out) {
    return lzham_decompress_memory(&tf2lzham_decompress_params, dst, dst_len, src, src_len, adler32_out, crc32_out);
}

extern "C" const char *tf2lzham_compress_strerror(uint32_t status) {
    switch (status) {
    // indeterminate
    case LZHAM_COMP_STATUS_NOT_FINISHED: return "incomplete: has more output in internal buffer"; break;
    case LZHAM_COMP_STATUS_NEEDS_MORE_INPUT: return "incomplete: needs more input"; break;
    case LZHAM_COMP_STATUS_HAS_MORE_OUTPUT: return "incomplete: has more output"; break;
    // success
    case LZHAM_COMP_STATUS_SUCCESS: return NULL; break;
    // errors (mostly unrecoverable)
    case LZHAM_COMP_STATUS_FAILED: return "compression failed"; break;
    case LZHAM_COMP_STATUS_FAILED_INITIALIZING: return "initialization failed"; break;
    case LZHAM_COMP_STATUS_INVALID_PARAMETER: return "invalid argument"; break;
    case LZHAM_COMP_STATUS_OUTPUT_BUF_TOO_SMALL: return "output buffer too small"; break;
    }
    return "unknown compression error";
}

extern "C" const char *tf2lzham_decompress_strerror(uint32_t status) {
    switch (status) {
    // indeterminate
    case LZHAM_DECOMP_STATUS_NOT_FINISHED: return "incomplete: more bytes available"; break;
    case LZHAM_DECOMP_STATUS_HAS_MORE_OUTPUT: return "incomplete: needs more output buffer space"; break;
    case LZHAM_DECOMP_STATUS_NEEDS_MORE_INPUT: return "incomplete: needs more input"; break;
    // success
    case LZHAM_DECOMP_STATUS_SUCCESS: return NULL; break;
    // errors (mostly unrecoverable)
    case LZHAM_DECOMP_STATUS_FAILED_INITIALIZING: return "initialization failed"; break;
    case LZHAM_DECOMP_STATUS_FAILED_DEST_BUF_TOO_SMALL: return "output buffer too small"; break;
    case LZHAM_DECOMP_STATUS_FAILED_EXPECTED_MORE_RAW_BYTES: return "unexpected end of file"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_CODE: return "bad code"; break;
    case LZHAM_DECOMP_STATUS_FAILED_ADLER32: return "failed adler32 checksum"; break;
    case LZHAM_DECOMP_STATUS_FAILED_CRC32: return "failed crc32 checksum"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_RAW_BLOCK: return "bad raw block"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_COMP_BLOCK_SYNC_CHECK: return "block out of sync"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_ZLIB_HEADER: return "bad zlib header"; break;
    case LZHAM_DECOMP_STATUS_FAILED_NEED_SEED_BYTES: return "needs seed bytes"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_SEED_BYTES: return "bad seed bytes"; break;
    case LZHAM_DECOMP_STATUS_FAILED_BAD_SYNC_BLOCK: return "bad sync block"; break;
    case LZHAM_DECOMP_STATUS_INVALID_PARAMETER: return "invalid argument"; break;
    }
    return "unknown decompression error";
}
