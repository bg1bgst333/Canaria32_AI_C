// main.cpp
// エントリーポイント: WinMain (ウィンドウ登録・作成・メッセージループ)

#include "PaintApp.h"
#include "Canvas.h"
#include "UI.h"
#include "WndProc.h"

// ================================================================
//  グローバル変数の実体定義
// ================================================================
AppState  g_app;
HWND      g_hwnd;
HINSTANCE g_hInst;

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
    wc.lpszClassName = _T("Canaria32_AI_C");
    wc.hIcon         = LoadIcon(NULL, IDI_APPLICATION);
    wc.hIconSm       = LoadIcon(NULL, IDI_APPLICATION);
    if (!RegisterClassEx(&wc)) return 1;

    HMENU hMenu = CreateAppMenu();
    g_hwnd = CreateWindowEx(
        0, _T("Canaria32_AI_C"), _T("Canaria32_AI_C - 新規"),
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
