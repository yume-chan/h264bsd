#!/bin/bash
set -e

SRC_DIR="source"
OBJ_DIR="obj"
LIB_OBJ="$OBJ_DIR/h264bsd.o"

DEBUG=false
if [[ "$DEBUG" == "true" ]]; then
    CFLAGS="-g3 -gsource-map=inline -Iinc -DH264DEC_TRACE -D_ERROR_PRINT"
    EXE_FLAGS=""
else
    CFLAGS="-flto -O3 -Iinc -sSTRICT"
    EXE_FLAGS="-sNO_DYNAMIC_EXECUTION -sEVAL_CTORS=2"
fi

echo "Compiling .c files from $SRC_DIR..."

mkdir -p "$OBJ_DIR"
rm -rf "$OBJ_DIR"/*.o

# Compile each .c file into its own .o
for cfile in "$SRC_DIR"/*.c; do
    ofile="$OBJ_DIR/$(basename "${cfile%.c}.o")"
    echo "  emcc -c $cfile -o $ofile"
    emcc $CFLAGS -c "$cfile" -Iinc -o "$ofile"
done

echo "Linking all objects into $LIB_OBJ..."

# Relocatable link into one .o file
emcc $CFLAGS -r "$OBJ_DIR"/*.o -o "$LIB_OBJ"

echo "Compiling to WebAssembly..."

mkdir -p wasm
rm -rf wasm/*

em++ -o wasm/h264bsd.mjs \
  $CFLAGS \
  $EXE_FLAGS \
  $LIB_OBJ \
  bind.cpp \
  -std=c++20 \
  -sEXIT_RUNTIME=0 \
  -sENVIRONMENT=web,worker \
  -sALLOW_MEMORY_GROWTH \
  -sIGNORE_MISSING_MAIN \
  -sTEXTDECODER=2 \
  -lembind \
  -sEMBIND_AOT \
  --emit-tsd h264bsd.d.ts

cp package.json NOTICE README.md wasm
