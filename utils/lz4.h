#ifndef __LZ4_H__
#define __LZ4_H__

size_t lz4_compress(const uint8_t *in, size_t inLength, uint8_t *out);
size_t lz4_decompress(const uint8_t *in, size_t inLength, uint8_t *out, size_t outLength);

#endif
