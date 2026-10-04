/**
 * Host directories behind the Wii's storage (src/pc/sdk/cnt.cpp, nand.cpp).
 */

#ifndef PC_FILES_H
#define PC_FILES_H

#include <types.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Directory with the channel's WAD contents, NN.app (NN = content index).
 * In order: PCSetContentsDir() (`--contents-dir`), the environment variable
 * NEWSCHANNEL_CONTENTS, ./orig/HAGE/contents, then the same path relative to
 * the repository the executable was built in (<exe>/../../orig/HAGE/contents).
 */
void PCSetContentsDir(const char* path);
const char* PCGetContentsDir(void);

/** TRUE if content `index` exists in the contents directory. */
BOOL PCContentExists(s32 index);

/**
 * Host directory that stands for the root of the Wii's NAND file system.
 * In order: PCSetNandDir() (`--nand-dir`), NEWSCHANNEL_NAND,
 * $XDG_DATA_HOME/newschannel/nand, ~/.local/share/newschannel/nand.
 * The directory is created when it is first needed. The channel's home
 * directory inside it is /title/00010002/48414745/data.
 */
void PCSetNandDir(const char* path);
const char* PCGetNandDir(void);

/** Converts a NAND path (absolute, or relative to the current NAND directory)
 * to a host path. Returns FALSE if the path is invalid or too long. */
BOOL PCNandHostPath(const char* nandPath, char* out, u32 outSize);

#ifdef __cplusplus
}
#endif

#endif /* PC_FILES_H */
