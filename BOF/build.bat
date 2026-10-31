@echo off
setlocal

set CLANGCL=..\..\LLVM-18.1.5\bin\clang-cl.exe

%CLANGCL% /c bof\illi_orc_bof.c /Foilli_orc_bof.x64.o /GS- /Gy /O1 /W3

if %errorlevel% == 0 (
    echo [+] illi_orc_bof.x64.o built successfully
) else (
    echo [-] Build failed
)