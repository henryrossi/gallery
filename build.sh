#!/bin/bash

# --- Unpack command line arguments
for arg in "$@"; do declare $arg='1'; done
if [ -z "${release+x}" ]; then debug=1; fi
if [ -n "${debug+x}" ];   then echo "[debug mode]"; fi
if [ -n "${release+x}" ]; then echo "[release mode]"; fi

./VulkanSDK/1.4.309.0/macOS/bin/glslc src/render/vulkan/rect.vert -o src/render/vulkan/vert.spv
./VulkanSDK/1.4.309.0/macOS/bin/glslc src/render/vulkan/rect.frag -o src/render/vulkan/frag.spv

# hr: add address sanitation
clang_common="-fsanitize=address -Wall -Wno-unused-function -I../src -L../thirdparty_lib"

link_os_graphics="-framework Cocoa -framework IOKit"
link_render="-rpath ../thirdparty_lib -lglfw3 -lvulkan"

clang_debug="clang -g -O0 ${clang_common}"
clang_release="clang -g -O2 ${clang_common}"

if [ -n "${debug+x}" ];   then compile="$clang_debug"; fi
if [ -n "${release+x}" ]; then compile="$clang_release"; fi

mkdir -p build

# --- Build all targets
cd build
if [ -n "${glyph+x}" ]; then built=1 && $compile ../src/glyph/glyph_main.c -o glyph $link_os_graphics $link_render; fi
if [ -n "${grove+x}" ]; then built=1 && $compile ../src/grove/grove_main.c -o grove $link_os_graphics $link_render; fi
if [ -n "${minze+x}" ]; then built=1 && $compile ../src/minze/minze_main.c -o minze; fi
cd ..

# --- Warn no targets built
if [ -z "${built+x}" ]; then
	echo "[Warning] no valid build target specified; specifiy build target names as arguments to this script, such as \`./build.sh grove\`."
	exit 1
fi
