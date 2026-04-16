@echo off
REM Build script for euryopa using Clang with MinGW-w64
REM Usage: build-clang.bat [Debug|Release]

setlocal

REM Set your Clang path here if not in PATH
REM set "CLANG_PATH=C:\Program Files\LLVM"
REM if exist "%CLANG_PATH%\bin" set PATH=%PATH%;%CLANG_PATH%\bin

REM Set your GLFW path (must have MinGW libraries in lib-mingw-w64)
set GLFW_DIR=..\glfw-3.3.4.bin.WIN64

REM Set your SDL2 path
set SDL2_DIR=..\SDL2-2.0.14

REM Clean old build
if exist "build" rmdir /s /q build
mkdir build

REM Generate makefiles for Clang
premake5 gmake2 --clangdir="C:/Program Files/LLVM" --glfwdir64=%GLFW_DIR% --sdl2dir=%SDL2_DIR%

if %ERRORLEVEL% NEQ 0 (
    echo Premake5 failed!
    exit /b 1
)

REM Build with Clang
cd build

REM Set CC and CXX for Clang if not in PATH
set CC=clang
set CXX=clang++

REM Configure for Clang
set CFLAGS=-target x86_64-pc-windows-gnu
set CXXFLAGS=-target x86_64-pc-windows-gnu -std=c++14

REM Build librwgta first
echo Building librwgta...
make -j4 librwgta
if %ERRORLEVEL% NEQ 0 (
    echo Failed to build librwgta!
    exit /b 1
)

REM Build euryopa
echo Building euryopa...
make -j4 euryopa
if %ERRORLEVEL% NEQ 0 (
    echo Failed to build euryopa!
    exit /b 1
)

echo.
echo Build complete!
echo Executable: build\bin\win-amd64-d3d9\Release\euryopa.exe
echo.

cd ..
endlocal
