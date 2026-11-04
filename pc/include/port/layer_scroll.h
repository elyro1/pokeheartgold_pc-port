#ifndef PORT_LAYER_SCROLL_H
#define PORT_LAYER_SCROLL_H

// Screen 0 = main, 1 = sub; layers 0-3 = BG, 4 = OBJ.
// Fixed-point sampling residual, in native pixels; zero restores DS sampling.
void SIM_Set2DLayerOffset(int screen, int layer, s32 x, s32 y);
// Independent OBJ priority groups, for sprites moving at different speeds.
void SIM_Set2DObjOffset(int screen, int priority, s32 x, s32 y);

#endif
