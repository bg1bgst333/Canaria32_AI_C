// PaintApp.cpp
// Win32 + C++03  ペイントアプリ
//   - 24bit BMP 読み書き
//   - フリーハンド描画 (ペン幅 1/3/5px)
//   - ChooseColor で色選択
//   - スクロールバー + マウスホイール対応

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

static AppState  g_app;
static HWND      g_hwnd;
static HINSTANCE g_hInst;

// ================================================================
//  キャンバス作成 (白で初期化)
// ================================================================
static BOOL CreateCanvas(int w, int h)
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
//  BMP 読み込み (24bit 非圧縮のみ)
// ================================================================
static BOOL LoadBMP(const TCHAR* path)
{
    HANDLE hFile = CreateFile(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                              OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD readBytes;
    BITMAPFILEHEADER bfh;
    ReadFile(hFile, &bfh, sizeof(bfh), &readBytes, NULL);
    if (readBytes != sizeof(bfh) || bfh.bfType != 0x4D42) {
        CloseHandle(hFile);
        return FALSE;
    }

    BITMAPINFOHEADER bih;
    ReadFile(hFile, &bih, sizeof(bih), &readBytes, NULL);
    if (readBytes != sizeof(bih)) { CloseHandle(hFile); return FALSE; }

    if (bih.biBitCount != 24 || bih.biCompression != BI_RGB) {
        CloseHandle(hFile);
        MessageBox(g_hwnd,
            _T("24ビット非圧縮BMPのみ対応しています。"),
            _T("エラー"), MB_OK | MB_ICONERROR);
        return FALSE;
    }

    int  w       = bih.biWidth;
    int  h       = (bih.biHeight < 0) ? -bih.biHeight : bih.biHeight;
    BOOL topDown = (bih.biHeight < 0);
    DWORD stride = (DWORD)((w * 3 + 3) & ~3);

    // top-down DIB を作成
    BITMAPINFO bmi;
    ZeroMemory(&bmi, sizeof(bmi));
    bmi.bmiHeader        = bih;
    bmi.bmiHeader.biHeight = -h;   // 強制的に top-down

    LPVOID pBits = NULL;
    HDC hdc = GetDC(NULL);
    HBITMAP hNew = CreateDIBSection(hdc, &bmi, DIB_RGB_COLORS, &pBits, NULL, 0);
    ReleaseDC(NULL, hdc);
    if (!hNew) { CloseHandle(hFile); return FALSE; }

    SetFilePointer(hFile, bfh.bfOffBits, NULL, FILE_BEGIN);
    BYTE* buf = (BYTE*)pBits;
    if (topDown) {
        for (int y = 0; y < h; y++)
            ReadFile(hFile, buf + y * stride, stride, &readBytes, NULL);
    } else {
        // ファイルは下から上順 → DIB は上から下順なので逆順に格納
        for (int y = h - 1; y >= 0; y--)
            ReadFile(hFile, buf + y * stride, stride, &readBytes, NULL);
    }
    CloseHandle(hFile);

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
//  BMP 書き込み (24bit bottom-up 標準形式)
// ================================================================
static BOOL SaveBMP(const TCHAR* path)
{
    if (!g_app.hBitmap) return FALSE;
    int   w      = g_app.canvasW;
    int   h      = g_app.canvasH;
    DWORD stride = (DWORD)((w * 3 + 3) & ~3);

    BITMAPFILEHEADER bfh;
    ZeroMemory(&bfh, sizeof(bfh));
    bfh.bfType    = 0x4D42;
    bfh.bfOffBits = sizeof(BITMAPFILEHEADER) + sizeof(BITMAPINFOHEADER);
    bfh.bfSize    = bfh.bfOffBits + stride * (DWORD)h;

    BITMAPINFOHEADER bih;
    ZeroMemory(&bih, sizeof(bih));
    bih.biSize        = sizeof(bih);
    bih.biWidth       = w;
    bih.biHeight      = h;          // bottom-up (BMP 標準)
    bih.biPlanes      = 1;
    bih.biBitCount    = 24;
    bih.biCompression = BI_RGB;
    bih.biSizeImage   = stride * (DWORD)h;

    HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL,
                              CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
    if (hFile == INVALID_HANDLE_VALUE) return FALSE;

    DWORD written;
    WriteFile(hFile, &bfh, sizeof(bfh), &written, NULL);
    WriteFile(hFile, &bih, sizeof(bih), &written, NULL);
    // DIB は top-down → BMP は bottom-up なので逆順に書く
    BYTE* buf = (BYTE*)g_app.pBits;
    for (int y = h - 1; y >= 0; y--)
        WriteFile(hFile, buf + y * stride, stride, &written, NULL);
    CloseHandle(hFile);
    return TRUE;
}

// ================================================================
//  スクロールバー
// ================================================================
static void ClampScroll(HWND hwnd)
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

static void UpdateScrollbars(HWND hwnd)
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

// ================================================================
//  描画ヘルパー
// ================================================================
static void DrawStroke(int x1, int y1, int x2, int y2)
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
//  名前を付けて保存ダイアログ
// ================================================================
static BOOL DoSaveAs(HWND hwnd)
{
    OPENFILENAME ofn;
    ZeroMemory(&ofn, sizeof(ofn));
    TCHAR path[MAX_PATH] = {0};
    if (g_app.filePath[0] != _T('\0'))
        lstrcpyn(path, g_app.filePath, MAX_PATH);

    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner   = hwnd;
    ofn.lpstrFilter = _T("ビットマップ (*.bmp)\0*.bmp\0");
    ofn.lpstrFile   = path;
    ofn.nMaxFile    = MAX_PATH;
    ofn.Flags       = OFN_OVERWRITEPROMPT;
    ofn.lpstrDefExt = _T("bmp");

    if (!GetSaveFileName(&ofn)) return FALSE;
    if (!SaveBMP(path)) {
        MessageBox(hwnd, _T("保存に失敗しました。"), _T("エラー"), MB_OK | MB_ICONERROR);
        return FALSE;
    }
    lstrcpyn(g_app.filePath, path, MAX_PATH);
    g_app.modified = FALSE;
    TCHAR title[MAX_PATH + 32];
    wsprintf(title, _T("ペイント - %s"), path);
    SetWindowText(hwnd, title);
    return TRUE;
}

// ================================================================
//  ウィンドウタイトル更新
// ================================================================
static void UpdateTitle(HWND hwnd)
{
    if (g_app.filePath[0] == _T('\0')) {
        SetWindowText(hwnd, _T("ペイント - 新規"));
    } else {
        TCHAR title[MAX_PATH + 32];
        wsprintf(title, _T("ペイント - %s"), g_app.filePath);
        SetWindowText(hwnd, title);
    }
}

// ================================================================
//  メニュー作成
// ================================================================
static HMENU CreateAppMenu()
{
    HMENU hMenu = CreateMenu();

    HMENU hFile = CreatePopupMenu();
    AppendMenu(hFile, MF_STRING, IDM_FILE_NEW,    _T("新規(&N)\tCtrl+N"));
    AppendMenu(hFile, MF_STRING, IDM_FILE_OPEN,   _T("開く(&O)\tCtrl+O"));
    AppendMenu(hFile, MF_STRING, IDM_FILE_SAVE,   _T("上書き保存(&S)\tCtrl+S"));
    AppendMenu(hFile, MF_STRING, IDM_FILE_SAVEAS, _T("名前を付けて保存(&A)"));
    AppendMenu(hFile, MF_SEPARATOR, 0, NULL);
    AppendMenu(hFile, MF_STRING, IDM_FILE_EXIT,   _T("終了(&X)"));

    HMENU hTool = CreatePopupMenu();
    AppendMenu(hTool, MF_STRING, IDM_TOOL_COLOR, _T("色の選択(&C)\tCtrl+K"));
    AppendMenu(hTool, MF_SEPARATOR, 0, NULL);
    AppendMenu(hTool, MF_STRING | MF_CHECKED, IDM_TOOL_PEN1, _T("細いペン  1px(&1)"));
    AppendMenu(hTool, MF_STRING,              IDM_TOOL_PEN3, _T("中ペン    3px(&3)"));
    AppendMenu(hTool, MF_STRING,              IDM_TOOL_PEN5, _T("太いペン  5px(&5)"));

    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hFile, _T("ファイル(&F)"));
    AppendMenu(hMenu, MF_POPUP, (UINT_PTR)hTool, _T("ツール(&T)"));
    return hMenu;
}

// ================================================================
//  ウィンドウプロシージャ
// ================================================================
LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    switch (msg)
    {
    // ------------------------------------------------------------
    case WM_ERASEBKGND:
        return 1;   // 背景消去を抑制 (ちらつき防止)

    case WM_SIZE:
        ClampScroll(hwnd);
        UpdateScrollbars(hwnd);
        InvalidateRect(hwnd, NULL, FALSE);
        return 0;

    // ------------------------------------------------------------
    case WM_PAINT:
    {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        RECT rc;
        GetClientRect(hwnd, &rc);

        // オフスクリーンバッファに合成してから一気に転送 (ダブルバッファ)
        HDC     hBufDC  = CreateCompatibleDC(hdc);
        HBITMAP hBufBmp = CreateCompatibleBitmap(hdc, rc.right, rc.bottom);
        HBITMAP hBufOld = (HBITMAP)SelectObject(hBufDC, hBufBmp);

        // 背景 (グレー)
        FillRect(hBufDC, &rc, (HBRUSH)(COLOR_APPWORKSPACE + 1));

        if (g_app.hBitmap) {
            HDC hCanvasDC = CreateCompatibleDC(hdc);
            HBITMAP hOld  = (HBITMAP)SelectObject(hCanvasDC, g_app.hBitmap);

            int srcX  = g_app.scrollX;
            int srcY  = g_app.scrollY;
            int drawW = g_app.canvasW - srcX;
            int drawH = g_app.canvasH - srcY;
            if (drawW > rc.right)  drawW = rc.right;
            if (drawH > rc.bottom) drawH = rc.bottom;

            if (drawW > 0 && drawH > 0)
                BitBlt(hBufDC, 0, 0, drawW, drawH, hCanvasDC, srcX, srcY, SRCCOPY);

            SelectObject(hCanvasDC, hOld);
            DeleteDC(hCanvasDC);
        }

        // バッファを画面に一括転送
        BitBlt(hdc, 0, 0, rc.right, rc.bottom, hBufDC, 0, 0, SRCCOPY);

        SelectObject(hBufDC, hBufOld);
        DeleteObject(hBufBmp);
        DeleteDC(hBufDC);

        EndPaint(hwnd, &ps);
        return 0;
    }

    // ------------------------------------------------------------
    case WM_LBUTTONDOWN:
    {
        int mx = GET_X_LPARAM(lParam) + g_app.scrollX;
        int my = GET_Y_LPARAM(lParam) + g_app.scrollY;
        SetCapture(hwnd);
        g_app.drawing = TRUE;
        g_app.lastX   = mx;
        g_app.lastY   = my;
        if (g_app.hBitmap) {
            DrawStroke(mx, my, mx, my);
            g_app.modified = TRUE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        return 0;
    }

    case WM_MOUSEMOVE:
    {
        if (!g_app.drawing) return 0;
        int mx = GET_X_LPARAM(lParam) + g_app.scrollX;
        int my = GET_Y_LPARAM(lParam) + g_app.scrollY;
        if (g_app.hBitmap) {
            DrawStroke(g_app.lastX, g_app.lastY, mx, my);
            g_app.modified = TRUE;
            InvalidateRect(hwnd, NULL, FALSE);
        }
        g_app.lastX = mx;
        g_app.lastY = my;
        return 0;
    }

    case WM_LBUTTONUP:
        if (g_app.drawing) {
            g_app.drawing = FALSE;
            ReleaseCapture();
        }
        return 0;

    // ------------------------------------------------------------
    case WM_HSCROLL:
    {
        SCROLLINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask  = SIF_ALL;
        GetScrollInfo(hwnd, SB_HORZ, &si);
        switch (LOWORD(wParam)) {
        case SB_LINELEFT:   si.nPos -= 16;           break;
        case SB_LINERIGHT:  si.nPos += 16;           break;
        case SB_PAGELEFT:   si.nPos -= (int)si.nPage; break;
        case SB_PAGERIGHT:  si.nPos += (int)si.nPage; break;
        case SB_THUMBTRACK: si.nPos  = si.nTrackPos; break;
        }
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_HORZ, &si, TRUE);
        GetScrollInfo(hwnd, SB_HORZ, &si);
        g_app.scrollX = si.nPos;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_VSCROLL:
    {
        SCROLLINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cbSize = sizeof(si);
        si.fMask  = SIF_ALL;
        GetScrollInfo(hwnd, SB_VERT, &si);
        switch (LOWORD(wParam)) {
        case SB_LINEUP:     si.nPos -= 16;           break;
        case SB_LINEDOWN:   si.nPos += 16;           break;
        case SB_PAGEUP:     si.nPos -= (int)si.nPage; break;
        case SB_PAGEDOWN:   si.nPos += (int)si.nPage; break;
        case SB_THUMBTRACK: si.nPos  = si.nTrackPos; break;
        }
        si.fMask = SIF_POS;
        SetScrollInfo(hwnd, SB_VERT, &si, TRUE);
        GetScrollInfo(hwnd, SB_VERT, &si);
        g_app.scrollY = si.nPos;
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    case WM_MOUSEWHEEL:
    {
        int delta = GET_WHEEL_DELTA_WPARAM(wParam);
        g_app.scrollY -= delta / 3;
        ClampScroll(hwnd);
        UpdateScrollbars(hwnd);
        InvalidateRect(hwnd, NULL, TRUE);
        return 0;
    }

    // ------------------------------------------------------------
    case WM_COMMAND:
    {
        switch (LOWORD(wParam))
        {
        case IDM_FILE_NEW:
        {
            if (g_app.modified) {
                if (MessageBox(hwnd,
                    _T("変更が保存されていません。新規作成しますか？"),
                    _T("確認"), MB_YESNO | MB_ICONQUESTION) != IDYES)
                    break;
            }
            CreateCanvas(DEFAULT_W, DEFAULT_H);
            g_app.filePath[0] = _T('\0');
            g_app.modified    = FALSE;
            ClampScroll(hwnd);
            UpdateScrollbars(hwnd);
            InvalidateRect(hwnd, NULL, TRUE);
            UpdateTitle(hwnd);
            break;
        }

        case IDM_FILE_OPEN:
        {
            if (g_app.modified) {
                if (MessageBox(hwnd,
                    _T("変更が保存されていません。開きますか？"),
                    _T("確認"), MB_YESNO | MB_ICONQUESTION) != IDYES)
                    break;
            }
            OPENFILENAME ofn;
            ZeroMemory(&ofn, sizeof(ofn));
            TCHAR path[MAX_PATH] = {0};
            ofn.lStructSize = sizeof(ofn);
            ofn.hwndOwner   = hwnd;
            ofn.lpstrFilter = _T("ビットマップ (*.bmp)\0*.bmp\0すべてのファイル (*.*)\0*.*\0");
            ofn.lpstrFile   = path;
            ofn.nMaxFile    = MAX_PATH;
            ofn.Flags       = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
            ofn.lpstrDefExt = _T("bmp");
            if (GetOpenFileName(&ofn)) {
                if (LoadBMP(path)) {
                    lstrcpyn(g_app.filePath, path, MAX_PATH);
                    g_app.modified = FALSE;
                    ClampScroll(hwnd);
                    UpdateScrollbars(hwnd);
                    InvalidateRect(hwnd, NULL, TRUE);
                    UpdateTitle(hwnd);
                } else {
                    MessageBox(hwnd, _T("ファイルを開けませんでした。"),
                               _T("エラー"), MB_OK | MB_ICONERROR);
                }
            }
            break;
        }

        case IDM_FILE_SAVE:
        {
            if (g_app.filePath[0] == _T('\0')) {
                DoSaveAs(hwnd);
            } else {
                if (!SaveBMP(g_app.filePath)) {
                    MessageBox(hwnd, _T("保存に失敗しました。"),
                               _T("エラー"), MB_OK | MB_ICONERROR);
                } else {
                    g_app.modified = FALSE;
                }
            }
            break;
        }

        case IDM_FILE_SAVEAS:
            DoSaveAs(hwnd);
            break;

        case IDM_FILE_EXIT:
            PostMessage(hwnd, WM_CLOSE, 0, 0);
            break;

        case IDM_TOOL_COLOR:
        {
            static COLORREF customColors[16];
            CHOOSECOLOR cc;
            ZeroMemory(&cc, sizeof(cc));
            cc.lStructSize  = sizeof(cc);
            cc.hwndOwner    = hwnd;
            cc.rgbResult    = g_app.penColor;
            cc.lpCustColors = customColors;
            cc.Flags        = CC_FULLOPEN | CC_RGBINIT;
            if (ChooseColor(&cc))
                g_app.penColor = cc.rgbResult;
            break;
        }

        case IDM_TOOL_PEN1:
            g_app.penWidth = 1;
            CheckMenuRadioItem(GetMenu(hwnd),
                IDM_TOOL_PEN1, IDM_TOOL_PEN5, IDM_TOOL_PEN1, MF_BYCOMMAND);
            break;
        case IDM_TOOL_PEN3:
            g_app.penWidth = 3;
            CheckMenuRadioItem(GetMenu(hwnd),
                IDM_TOOL_PEN1, IDM_TOOL_PEN5, IDM_TOOL_PEN3, MF_BYCOMMAND);
            break;
        case IDM_TOOL_PEN5:
            g_app.penWidth = 5;
            CheckMenuRadioItem(GetMenu(hwnd),
                IDM_TOOL_PEN1, IDM_TOOL_PEN5, IDM_TOOL_PEN5, MF_BYCOMMAND);
            break;
        }
        return 0;
    }

    // ------------------------------------------------------------
    case WM_CLOSE:
        if (g_app.modified) {
            if (MessageBox(hwnd,
                _T("変更が保存されていません。終了しますか？"),
                _T("確認"), MB_YESNO | MB_ICONQUESTION) != IDYES)
                return 0;
        }
        DestroyWindow(hwnd);
        return 0;

    case WM_DESTROY:
        if (g_app.hBitmap) DeleteObject(g_app.hBitmap);
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

// ================================================================
//  WinMain
// ================================================================
int WINAPI WinMain(HINSTANCE hInst, HINSTANCE, LPSTR, int nCmdShow)
{
    g_hInst = hInst;
    ZeroMemory(&g_app, sizeof(g_app));
    g_app.penColor = RGB(0, 0, 0);
    g_app.penWidth = 1;

    WNDCLASSEX wc;
    ZeroMemory(&wc, sizeof(wc));
    wc.cbSize        = sizeof(wc);
    wc.style         = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc   = WndProc;
    wc.hInstance     = hInst;
    wc.hCursor       = LoadCursor(NULL, IDC_CROSS);
    wc.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);
    wc.lpszClassName = _T("PaintApp");
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) return 1;

    HMENU hMenu = CreateAppMenu();
    g_hwnd = CreateWindowEx(
        0, _T("PaintApp"), _T("ペイント - 新規"),
        WS_OVERLAPPEDWINDOW | WS_HSCROLL | WS_VSCROLL,
        CW_USEDEFAULT, CW_USEDEFAULT, 960, 720,
        NULL, hMenu, hInst, NULL);
    if (!g_hwnd) return 1;

    CreateCanvas(DEFAULT_W, DEFAULT_H);
    UpdateScrollbars(g_hwnd);

    // アクセラレータキー
    ACCEL accel[] = {
        { FCONTROL | FVIRTKEY, 'N', IDM_FILE_NEW   },
        { FCONTROL | FVIRTKEY, 'O', IDM_FILE_OPEN  },
        { FCONTROL | FVIRTKEY, 'S', IDM_FILE_SAVE  },
        { FCONTROL | FVIRTKEY, 'K', IDM_TOOL_COLOR },
    };
    HACCEL hAccel = CreateAcceleratorTable(accel, 4);

    ShowWindow(g_hwnd, nCmdShow);
    UpdateWindow(g_hwnd);

    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        if (!TranslateAccelerator(g_hwnd, hAccel, &msg)) {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }
    DestroyAcceleratorTable(hAccel);
    return (int)msg.wParam;
}
