// FileIO.cpp
// BMP ファイル I/O

#include "FileIO.h"

// ================================================================
//  BMP 読み込み (24bit 非圧縮のみ)
// ================================================================
BOOL LoadBMP(const TCHAR* path)
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
BOOL SaveBMP(const TCHAR* path)
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
