@echo off

setlocal

if "%CC%" == "" set CC=cc
set _CFLAGS=%CFLAGS% -O2 -DLIBPORTAL_PLATFORM_HAS_GDI
set _LDFLAGS=%LDFLAGS% -lws2_32 -lgdi32

set SCRIPT_ROOT=%~dp0
set SRC_DIR=%SCRIPT_ROOT%\src
set TOOL_DIR=%SCRIPT_ROOT%\tool

%CC% -o bin2c.exe "%TOOL_DIR%\bin2c.c" || echo "cannot compile bin2c" && exit /b 1

bin2c -o res_indexhtml.h -n indexhtml "%SRC_DIR%\index.html" || echo "cannot generate resource from index.html" && exit /b 1

echo "CFLAGS: %_CFLAGS%"
echo "LDFLAGS: %_LDFLAGS%"

%CC% -o portalweb.exe -I. %_CFLAGS% "%SRC_DIR%\main.c" %_LDFLAGS% || echo "compilation failed" && exit /b 1

