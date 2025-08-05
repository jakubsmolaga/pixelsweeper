CFLAGS = -xc -std=c99 --target=wasm32 -nostdlib -nodefaultlibs -fno-builtin
LFLAGS = -Wl,--no-entry -Wl,--export-all

debug:
	@clang $(CFLAGS) $(LFLAGS) -g -o main.wasm main.c

release:
	@clang $(CFLAGS) $(LFLAGS) -Os -o main.wasm main.c
