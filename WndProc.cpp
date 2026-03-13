// WndProc.cpp
// ウィンドウプロシージャ

#include "WndProc.h"
#include "Canvas.h"
#include "FileIO.h"
#include "UI.h"

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
