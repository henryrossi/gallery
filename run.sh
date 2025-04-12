#!/bin/sh

flags=$(cat compile_flags.txt)

gcc -o glyph glyph.c glad.c $flags -g
./glyph
