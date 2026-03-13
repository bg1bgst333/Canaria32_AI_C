// Canvas.cpp
// キャンバス生成・描画・スクロール管理

#include "Canvas.h"

// ================================================================
//  キャンバス作成 (白で初期化)
// ================================================================
BOOL CreateCanvas(int w, int h)
{
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader.biSize        = sizeof(BITMAPINFOHEADER);
    bmi.bmiHeader.biWidth       = w;
    bmi.bmiHeader.biHeight      = -h;   // top-down
    bmi.bmiHeader.biPlanes      = 1;
    bmi.bmiHeader.biBitCount    = 24;
    bmi.bmiHeader.biCompression = BI_RGB;

    LPVOID pBits = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hNew = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!hNew) return FALSE;

    DWORD stride = (DWORD)((w * 3 + 3) & ~3);
    memset(pBits, 0xFF, stride * (DWORD)h);   // 白

    if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
    g_app.hBitmap = hNew;
    g_app.pBits   = pBits;
    g_app.canvasW = w;
    g_app.canvasH = h;
    g_app.scrollX = 0;
    g_app.scrollY = 0;
    return TRUE;
}

// ================================================================
//  描画ヘルパー
// ================================================================
void DrawStroke(int x1, int y1, int x2, int y2)
{
    HDC hMemDC = CreateCompatibleDC(NULL);
    HBITMAP hOld = (HBITMAP)SelectObject(hMemDC, g_app.hBitmap);

    if (x1 == x2 && y1 == y2) {
        // 点だけ: 塗りつぶし円
        HBRUSH hBrush    = CreateSolidBrush(g_app.penColor);
        HBRUSH hOldBrush = (HBRUSH)SelectObject(hMemDC, hBrush);
        HPEN   hOldPen   = (HPEN)SelectObject(hMemDC, GetStockObject(NULL_PEN));
        int r = g_app.penWidth / 2;
        Ellipse(hMemDC, x1 - r, y1 - r, x1 + r + 1, y1 + r + 1);
        SelectObject(hMemDC, hOldBrush);
        SelectObject(hMemDC, hOldPen);
        DeleteObject(hBrush);
    } else {
        // 線分: 丸端キャップ
        LOGBRUSH lb;
        lb.lbStyle = BS_SOLID;
        lb.lbColor = g_app.penColor;
        lb.lbHatch = 0;
        HPEN hPen    = ExtCreatePen(
            PS_GEOMETRIC | PS_SOLID | PS_ENDCAP_ROUND | PS_JOIN_ROUND,
            (DWORD)g_app.penWidth, &lb, 0, NULL);
        HPEN hOldPen = (HPEN)SelectObject(hMemDC, hPen);
        MoveToEx(hMemDC, x1, y1, NULL);
        LineTo(hMemDC, x2, y2);
        SelectObject(hMemDC, hOldPen);
        DeleteObject(hPen);
    }

    SelectObject(hMemDC, hOld);
    DeleteDC(hMemDC);
}

// ================================================================
//  スクロールバー
// ================================================================
void ClampScroll(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);
    int maxX = g_app.canvasW - rc.right;
    int maxY = g_app.canvasH - rc.bottom;
    if (maxX < 0) maxX = 0;
    if (maxY < 0) maxY = 0;
    if (g_app.scrollX < 0)    g_app.scrollX = 0;
    if (g_app.scrollX > maxX) g_app.scrollX = maxX;
    if (g_app.scrollY < 0)    g_app.scrollY = 0;
    if (g_app.scrollY > maxY) g_app.scrollY = maxY;
}

void UpdateScrollbars(HWND hwnd)
{
    RECT rc;
    GetClientRect(hwnd, &rc);

    SCROLLINFO si;
    ZeroMemory(&si, sizeof(si));
    si.cbSize = sizeof(si);
    si.fMask  = SIF_RANGE | SIF_PAGE | SIF_POS;

    si.nMin  = 0;
    si.nMax  = g_app.canvasW > 0 ? g_app.canvasW - 1 : 0;
    si.nPage = (UINT)rc.right;
    si.nPos  = g_app.scrollX;
    SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);

    si.nMin  = 0;
    si.nMax  = g_app.canvasH > 0 ? g_app.canvasH - 1 : 0;
    si.nPage = (UINT)rc.bottom;
    si.nPos  = g_app.scrollY;
    SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
}
