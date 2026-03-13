// UI.cpp
// メニュー・ダイアログ・タイトル

#include "UI.h"
#include "FileIO.h"

// ================================================================
//  メニュー作成
// ================================================================
HMENU CreateAppMenu()
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
//  名前を付けて保存ダイアログ
// ================================================================
BOOL DoSaveAs(HWND hwnd)
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
void UpdateTitle(HWND hwnd)
{
    if (g_app.filePath[0] == _T('\0')) {
        SetWindowText(hwnd, _T("ペイント - 新規"));
    } else {
        TCHAR title[MAX_PATH + 32];
        wsprintf(title, _T("ペイント - %s"), g_app.filePath);
        SetWindowText(hwnd, title);
    }
}
