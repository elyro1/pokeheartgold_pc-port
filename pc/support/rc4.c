#include "nitrocrypto_rc4.h"

/* Native equivalent of lib/src/nitrocrypto_rc4.c's ARM byte-state loop. */
void CRYPTO_RC4Encrypt(void *state, const void *input, u32 length, void *output) {
    CRYPTORC4Context *context = state;
    const u8 *source = input;
    u8 *destination = output;
    u8 i = context->i;
    u8 j = context->j;
    while (length--) {
        i++;
        u8 a = context->s[i];
        j += a;
        u8 b = context->s[j];
        context->s[i] = b;
        context->s[j] = a;
        *destination++ = *source++ ^ context->s[(u8)(a + b)];
    }
    context->i = i;
    context->j = j;
}
