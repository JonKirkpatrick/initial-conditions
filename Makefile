# to compile and run in one command type:
# make run

# define which compiler to use
CXX     := g++
OUTPUT  := sfmlgame
OS      := $(shell uname)
SRC_DIR := ./src
OBJ_DIR := ./build
BIN_DIR := ./bin

# linux compiler / linker flags
ifeq ($(OS), Linux)
	CXX_FLAGS := -O3 -g -std=c++23 -Wno-unused-result -Wno-deprecated-declarations -DGLEW_STATIC -fno-omit-frame-pointer -fopenmp
	INCLUDES  := -I$(SRC_DIR) -I$(SRC_DIR)/thirdparty
	LDFLAGS   := -L/usr/local/lib -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -lGLEW -lGL -fopenmp
endif

# mac osx compiler / linker flags
ifeq ($(OS), Darwin)
    SFML_DIR  := /opt/homebrew/Cellar/sfml/3.0.1
    CXX_FLAGS := -O3 -std=c++23 -Wno-unused-result -Wno-deprecated-declarations -DGLEW_STATIC -Xpreprocessor -fopenmp -I/opt/homebrew/opt/libomp/include
	LDFLAGS   := -O3 -lsfml-graphics -lsfml-window -lsfml-system -lsfml-audio -L$(SFML_DIR)/lib -framework OpenGL -L/opt/homebrew/opt/libomp/lib -lomp
    INCLUDES  := -I$(SRC_DIR) -I$(SRC_DIR)/thirdparty -I$(SFML_DIR)/include
endif

# 1. FIND ALL SOURCE FILES RECURSIVELY
# Using rwildcard lets GNU Make search all nested folders automatically
rwildcard = $(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))

SRC_FILES_CPP := $(call rwildcard,$(SRC_DIR)/,*.cpp)
SRC_FILES_C   := $(call rwildcard,$(SRC_DIR)/,*.c)

# 2. MAP SOURCE FILES TO THE BUILD DIRECTORY
# This transforms src/renderer/Camera.cpp into build/renderer/Camera.o
OBJ_FILES := $(patsubst $(SRC_DIR)/%.cpp,$(OBJ_DIR)/%.o,$(SRC_FILES_CPP)) \
             $(patsubst $(SRC_DIR)/%.c,$(OBJ_DIR)/%.o,$(SRC_FILES_C))

# Include dependency files automatically
DEP_FILES := $(OBJ_FILES:.o=.d)
-include $(DEP_FILES)

all: $(OUTPUT)

# Link the final executable into the /bin folder
$(OUTPUT): $(OBJ_FILES) Makefile
	@mkdir -p $(BIN_DIR)
	$(CXX) $(OBJ_FILES) $(LDFLAGS) -o $(BIN_DIR)/$@

# Compile C++ files and mirror the directory structure inside /build
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.cpp
	@mkdir -p $(dir $@)
	$(CXX) -MMD -MP -c $(CXX_FLAGS) $(INCLUDES) $< -o $@

# Compile C files and mirror the directory structure inside /build
$(OBJ_DIR)/%.o: $(SRC_DIR)/%.c
	@mkdir -p $(dir $@)
	$(CXX) -MMD -MP -c $(CXX_FLAGS) $(INCLUDES) $< -o $@

# Clean now safely wipes the dedicated build and bin directories
clean:
	rm -rf $(OBJ_DIR)
	rm -f $(BIN_DIR)/$(OUTPUT)

run: $(OUTPUT)
	cd $(BIN_DIR) && ./$(OUTPUT)