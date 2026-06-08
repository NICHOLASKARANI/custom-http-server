@echo off
echo Building Custom HTTP Server for Windows...

if not exist "build" mkdir build

cl /nologo /O2 /MT /W3 /D_WIN32_WINNT=0x0601 /Iinclude src\main_win.c src\server_win.c /Fe:build\http-server.exe /link ws2_32.lib kernel32.lib

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Executable created: build\http-server.exe
    echo.
    echo To run the server:
    echo   build\http-server.exe
    echo.
    echo Or double-click build\http-server.exe
) else (
    echo.
    echo Build failed. Make sure you have Visual Studio Build Tools installed.
    echo.
    echo If you don't have cl.exe, you can use MinGW:
    echo   gcc -O2 -o build\http-server.exe src\main_win.c src\server_win.c -lws2_32
)

pause