#ifndef HG_PC_QSORT_H
#define HG_PC_QSORT_H
#include <stdlib.h>
/* These reconstructed callers use const comparators. Host qsort has that exact
 * contract and owns its workspace; the DS SDK's workspace stays unused on PC. */
static inline void HG_QSort(void *base, unsigned int count, unsigned int width,
                            int (*compare)(const void *, const void *), void *workspace) {
    (void)workspace;
    qsort(base, count, width, compare);
}
#endif
