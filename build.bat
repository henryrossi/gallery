@echo off

:: --- Unpack command line arguments
for %%a in (%*) do set "%%~a=1"
if not "%release%"=="1" set debug=1
if "%debug%"=="1"   set release=0 && echo [debug mode]
if "%release%"=="1" set debug=0 && echo [release mode]

set clang_common= -Wall -Wno-deprecated-declarations -Wno-unused-function -I../src -I../src/thirdparty -L../thirdparty_lib

set link_render= -lglfw3dll -lvulkan-1"

set clang_debug=   call clang -g -O0 %clang_common% 
set clang_release= call clang -g -O2 %clang_common% 

if "%debug%"=="1" set compile=%clang_debug%
if "%release%"=="1" set compile=%clang_release%

if not exist build mkdir build

:: ---  Build all targets
pushd build
if "%glyph%"=="1" set built=1 && %compile% ../src/glyph/glyph_main.c -o glyph.exe  %link_render%
if "%grove%"=="1" set built=1 && %compile% ../src/grove/grove_main.c -o grove.exe  %link_render%
if "%minze%"=="1" set built=1 && %compile% ../src/minze/minze_main.c -o minze.exe  %link_render%
:: hr: need warning about this failing
if not exist glfw3.dll copy "..\thirdparty_lib\glfw3.dll" "glfw3.dll"
popd

:: --- Warn no targets built
if "%built%"=="" (
	echo [Warning] no valid build target specified; specifiy build target names as arguments to this script, such as `./build.sh grove`.
	exit /b 1
)

