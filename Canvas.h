// Canvas.h
// キャンバス生成・描画・スクロール管理

#ifndef CANVAS_H
#define CANVAS_H

#include "PaintApp.h"

BOOL CreateCanvas(int w, int h);
void DrawStroke(int x1, int y1, int x2, int y2);
void ClampScroll(HWND hwnd);
void UpdateScrollbars(HWND hwnd);

#endif // CANVAS_H
