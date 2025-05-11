#!/bin/sh

./VulkanSDK/1.4.309.0/macOS/bin/glslc shaders/shader.vert -o shaders/vert.spv
./VulkanSDK/1.4.309.0/macOS/bin/glslc shaders/shader.frag -o shaders/frag.spv

flags=$(cat compile_flags.txt)

gcc -o glyph src/glyph.c $flags -g -Wall -Werror
./glyph
