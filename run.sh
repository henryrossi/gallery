#!/bin/sh

flags=$(cat compile_flags.txt)

gcc -o glyph src/glyph.c $flags -g -Wall -Werror
./glyph
