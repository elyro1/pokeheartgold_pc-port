#ifndef HG_PC_RUNTIME_DIRECTORY_H
#define HG_PC_RUNTIME_DIRECTORY_H

// Packaged builds keep their files beside the executable. Development builds
// can keep using the existing run/ folder populated by Makefile.pc assets.
void HG_PC_UseRuntimeDirectory();

#endif
