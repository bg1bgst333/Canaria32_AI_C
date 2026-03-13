// PaintApp.h
// 共通定義: AppState, メニューID, 定数, グローバル変数 extern 宣言

#ifndef PAINTAPP_H
#define PAINTAPP_H

#define UNICODE
#define _UNICODE
#include <windows.h>
#include <windowsx.h>   // GET_X_LPARAM / GET_Y_LPARAM
#include <commdlg.h>
#include <tchar.h>

#ifdef _MSC_VER
#pragma comment(lib, "comdlg32.lib")
#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "user32.lib")
#endif

// ================================================================
//  メニュー ID
// ================================================================
enum {
    IDM_FILE_NEW    = 101,
    IDM_FILE_OPEN,
    IDM_FILE_SAVE,
    IDM_FILE_SAVEAS,
    IDM_FILE_EXIT,
    IDM_TOOL_COLOR  = 201,
    IDM_TOOL_PEN1,
    IDM_TOOL_PEN3,
    IDM_TOOL_PEN5
};

static const int DEFAULT_W = 800;
static const int DEFAULT_H = 600;

// ================================================================
//  アプリ状態
// ================================================================
struct AppState {
    HBITMAP  hBitmap;
    LPVOID   pBits;
    int      canvasW, canvasH;
    int      scrollX, scrollY;
    COLORREF penColor;
    int      penWidth;
    BOOL     drawing;
    int      lastX, lastY;
    TCHAR    filePath[MAX_PATH];
    BOOL     modified;
};

extern AppState  g_app;
extern HWND      g_hwnd;
extern HINSTANCE g_hInst;

#endif // PAINTAPP_H
