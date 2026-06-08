@echo off
echo Building Custom HTTP Server with MinGW...

if not exist "build" mkdir build

gcc -O2 -D_WIN32_WINNT=0x0601 -o build\http-server.exe src\main_win.c src\server_win.c -lws2_32

if %errorlevel% equ 0 (
    echo.
    echo Build successful! Executable created: build\http-server.exe
    echo.
    echo To run the server:
    echo   build\http-server.exe
) else (
    echo.
    echo Build failed. Make sure MinGW is installed.
)

pause