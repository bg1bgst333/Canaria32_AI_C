// FileIO.h
// BMP ファイル I/O

#ifndef FILEIO_H
#define FILEIO_H

#include "PaintApp.h"

BOOL LoadBMP(const TCHAR* path);
BOOL SaveBMP(const TCHAR* path);

#endif // FILEIO_H
