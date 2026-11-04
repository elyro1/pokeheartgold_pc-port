#include "nitrocrypto_rc4.h"

/* Preserve the original fast context's high-byte permutation and counters. */
void CRYPTO_RC4FastEncrypt(void *state, const void *input, u32 length, void *output) {
    CRYPTORC4FastContext *context = state;
    const u8 *source = input;
    u8 *destination = output;
    u32 i = context->i;
    u32 j = context->j;
    while (length--) {
        i += 1u << 24;
        u32 a = context->s[i >> 24];
        j += a;
        u32 b = context->s[j >> 24];
        context->s[i >> 24] = b;
        context->s[j >> 24] = a;
        *destination++ = *source++ ^ (u8)(context->s[(a + b) >> 24] >> 24);
    }
    context->i = i;
    context->j = j;
}
