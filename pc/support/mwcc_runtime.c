#include <stdint.h>

/* Native versions of the Metrowerks runtime helpers the DS code calls directly.
 * _s32_div_f/_u32_div_f use the ARM9 hardware divider; its division by zero
 * result (GBATEK: -1 for a non-negative numerator, +1 for a negative one) is
 * kept so callers see the same value, and INT32_MIN / -1 wraps instead of
 * trapping. */
int32_t _s32_div_f(int32_t numerator, int32_t denominator) {
    if (denominator == 0) {
        return numerator < 0 ? 1 : -1;
    }
    return (int32_t)((int64_t)numerator / denominator);
}

uint32_t _u32_div_f(uint32_t numerator, uint32_t denominator) {
    return denominator == 0 ? 0xFFFFFFFFu : numerator / denominator;
}

float _f_lltof(int64_t value) {
    return (float)value;
}

/* Soft-float int to float: the DS passes floats as their IEEE bits in r0. */
uint32_t _fflt(int32_t value) {
    float result = (float)value;
    uint32_t bits;
    __builtin_memcpy(&bits, &result, sizeof(bits));
    return bits;
}

int64_t _ll_mul(int64_t a, int64_t b) { return a * b; }
