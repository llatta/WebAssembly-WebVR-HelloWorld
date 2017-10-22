CC = emcc
SRCS = main.c
FILES = $(addprefix src/, $(SRCS)) # Add 'src/' to each source
OBJS = $(FILES:.c=.o) # Modify file extensions of FILES
SHELLFILE = src/vr_template.html # shell_minimal.html # 
EOPT = WASM=1 USE_GLFW=3 # USE_WEBGL2=1 FULL_ES3=1  # Emscripten specific options
EOPTS = $(addprefix -s $(EMPTY), $(EOPT)) # -s NO_EXIT_RUNTIME=1	# Add '-s ' to each option

# Builds necessary files
build: $(OBJS) $(SHELLFILE)
		mkdir -p build
		$(CC) $(OBJS) $(EOPTS) -o build/index.html --shell-file $(SHELLFILE)

# Removes object files, but leaves build for serving
dist: build
		rm $(OBJS)

# Cleans up object files and build directory
clean:
		rm -rf build
		rm $(OBJS)
