#ifndef SHA3_AVX2_H
#define SHA3_AVX2_H

#include <stddef.h>
#include <stdint.h>

/* Check if AVX2 is available on this CPU */
int sha3_avx2_available(void);

/* One-shot SHA3-256 computation with AVX2 optimization */
void sha3_256_avx2(const uint8_t *data, size_t len, uint8_t output[32]);

#endif /* SHA3_AVX2_H */