// UI.h
// メニュー・ダイアログ・タイトル

#ifndef UI_H
#define UI_H

#include "PaintApp.h"

HMENU CreateAppMenu();
BOOL  DoSaveAs(HWND hwnd);
void  UpdateTitle(HWND hwnd);

#endif // UI_H
