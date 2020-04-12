#!/bin/bash
emcc -std=c11 ./*.c -o index.html -s MAX_WEBGL_VERSION=2 -s USE_SDL=2 -g4 --preload-file files -g4

