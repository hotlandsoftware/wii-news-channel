// Game data read at run time from the user's main.dol (dol_data.cpp).

#ifndef PC_DOL_DATA_H
#define PC_DOL_DATA_H

// The DOL: `--dol FILE`, else $NEWSCHANNEL_DOL, else orig/HAGE/sys/main.dol.
void PCDolDataSetPath(const char* path);
const char* PCDolDataGetPath();

// Fills gErrorSystemArc, the language and weekday name tables and
// SlideShow.cpp's lbl_80356940. Prints what is wrong and returns false if the
// file is missing or is not the HAGE v7 DOL. Must run before the game's main().
bool PCDolDataLoad();
bool PCDolDataIsLoaded();

#endif
