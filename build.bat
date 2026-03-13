@echo off
:: ============================================================
:: PaintApp ビルドスクリプト  (Visual C++ / MSVC)
::
:: 使い方:
::   1) Visual Studio の "開発者コマンドプロンプト" を開く
::      (または vcvarsall.bat を実行済みの環境)
::   2) このスクリプトを実行:  build.bat
::   3) 成功すると PaintApp.exe が生成される
:: ============================================================

set SRC=main.cpp Canvas.cpp FileIO.cpp UI.cpp WndProc.cpp
set OUT=PaintApp.exe
set CFLAGS=/nologo /W3 /O2 /MT /EHsc
set LFLAGS=/subsystem:windows /entry:WinMainCRTStartup
set LIBS=user32.lib gdi32.lib comdlg32.lib

echo ビルド中: %SRC% ...
cl %CFLAGS% %SRC% /Fe%OUT% /link %LFLAGS% %LIBS%

if %ERRORLEVEL% == 0 (
    echo.
    echo ビルド成功: %OUT%
) else (
    echo.
    echo ビルド失敗 (エラーコード: %ERRORLEVEL%)
    exit /b %ERRORLEVEL%
)
