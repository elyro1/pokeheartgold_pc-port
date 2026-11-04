#ifndef HG_PC_G3D_RENDER_INTERNAL_H
#define HG_PC_G3D_RENDER_INTERNAL_H
#include <nnsys/g3d_internal.h>
static inline G3dResMat *G3dGetResMat(const NNSG3dResMdl *pResMdl) {
    if (pResMdl) {
        u32 ofsMat = pResMdl->ofsMat;
        if (ofsMat) {
            return (G3dResMat *)((u8 *)pResMdl + ofsMat);
        }
    }
    return NULL;
}


#endif
