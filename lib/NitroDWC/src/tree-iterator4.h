#ifndef DWC_TREE_ITERATOR4_H
#define DWC_TREE_ITERATOR4_H
typedef struct Iterator {
    void *current;
#ifdef __cplusplus
    void Advance();
#endif
} Iterator;
#if defined(SDK_PORT)
typedef char DwcTreeIteratorNativeSize[sizeof(Iterator)==sizeof(void*)?1:-1];
#else
typedef char DwcTreeIteratorSize4[sizeof(Iterator)==4?1:-1];
#endif
#ifndef __cplusplus
void LobbyTreeIterator_Advance(Iterator *);
#endif
#endif
