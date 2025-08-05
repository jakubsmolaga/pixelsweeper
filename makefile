CFLAGS = -xc -std=c99 --target=wasm32 -g -nostdlib
LFLAGS = -Wl,--no-entry -Wl,--export-all

main.wasm: main.c
	@clang $(CFLAGS) $(LFLAGS) -o main.wasm main.c
