CC = emcc
SRCS = main.c lodepng.c
FILES = $(addprefix src/, $(SRCS)) # Add 'src/' to each source
OBJS = $(FILES:.c=.o)# Modify file extensions of FILES
EOPT = USE_WEBGL2=1 FULL_ES3=1 USE_GLFW=3 WASM=1 ASSERTIONS=1 # Emscripten specific options
EOPTS = $(addprefix -s $(EMPTY), $(EOPT)) --preload-file assets	# Add '-s ' to each option

.PHONY: dist clean

# Builds necessary files
build: $(OBJS)
		mkdir -p build
		emcc $(FILES) -O3 $(EOPTS) -o build/index.js

# Removes object files, but leaves build for serving
dist: build
		rm -f $(OBJS)

# Cleans up object files and build directory
clean:
		rm -rf build
		rm -f $(OBJS)
