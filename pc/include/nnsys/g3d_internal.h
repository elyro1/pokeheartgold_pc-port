#ifndef HG_PC_G3D_INTERNAL_H
#define HG_PC_G3D_INTERNAL_H
#include <nnsys/g3d.h>
// material block of a model (NNSG3dResMdl.ofsMat)
typedef struct G3dResMat {
    u16 ofsDictTexToMatList;  // texture name -> G3dResBindEntry
    u16 ofsDictPlttToMatList; // palette name -> G3dResBindEntry
    NNSG3dResDict dict;       // material -> offset of its G3dResMatData
} G3dResMat;

typedef struct G3dResMatData {
    u16 itemTag; // selects the SBC MAT handler
    u16 size;
    u32 prmMatColor0;    // 0x04, diffuse, ambient
    u32 prmMatColor1;    // 0x08, specular, emission
    u32 prmPolygonAttr;  // 0x0C
    u32 polygonAttrMask; // 0x10, bits taken from prmPolygonAttr (the rest from the global state)
    u32 texImageParam;   // 0x14
    u32 texImageParamMask;
    u16 texPlttBase; // 0x1C
    u16 flag;        // 0x1E, NNSG3dMatFlag
    u16 origWidth;   // 0x20
    u16 origHeight;  // 0x22
    fx32 scaleS;     // 0x24, texture width / origWidth
    fx32 scaleT;     // 0x28, texture height / origHeight
    u8 texMtxData[]; // 0x2C, present texture matrix parts: scale (2 fx32), rotation (2 fx16), translation (2 fx32)
} G3dResMatData;

#define G3D_RESMATDATA_TEXIMAGEPARAM_KEEP_MASK 0xC00F0000 // bits not set by a texture binding

// the materials one texture or palette is bound to
typedef struct G3dResBindEntry {
    u16 ofsMatIdxList; // from the G3dResMat, u8 material indices
    u8 numMat;
    u8 flag; // G3D_RESBIND_BOUND
} G3dResBindEntry;

#define G3D_RESBIND_BOUND 1

// NNSG3dResTex.dict entry
typedef struct G3dResTexData {
    u32 texImageParam;
    u32 extraParam; // width in bits 0-10, height in bits 11-21
} G3dResTexData;

#define G3D_RESTEXDATA_WIDTH(extraParam)  ((extraParam) & 0x7FF)
#define G3D_RESTEXDATA_HEIGHT(extraParam) (((extraParam) & 0x3FF800) >> 11)

// NNSG3dResTex.plttInfo.ofsDict dictionary entry
typedef struct G3dResPlttData {
    u16 offset;
    u16 flag; // G3D_RESPLTTDATA_4COLOR
} G3dResPlttData;

#define G3D_RESPLTTDATA_4COLOR 1 // offset in 8-byte units; other palettes are placed in 16-byte units


#endif
