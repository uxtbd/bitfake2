CXX      ?= g++
CXXFLAGS += -std=c++17 -Wall -Wextra -I./Utilities -Wno-deprecated-declarations
LDFLAGS  += -ltag -lfftw3 -lebur128 -lsndfile -lavformat -lavcodec -lswresample -lavutil -lcurl

BUILD_DIR := build
DESTDIR   ?= 
PREFIX    ?= /usr/local

MINGW_BIN := /mingw64/bin
UNAME_S := $(shell uname -s)
SRC := main.cpp helperfunctions.cpp filechecks.cpp globals.cpp nonusrfunctions.cpp coreoperations.cpp onlineoperations.cpp Utilities/pugixml.cpp
OBJ := $(addprefix $(BUILD_DIR)/,$(SRC:.cpp=.o))

ifeq ($(findstring MINGW,$(UNAME_S)),MINGW)
    WINDOWS := 1
    MINGW_BIN := /mingw64/bin
    EXE := .exe
else
    WINDOWS := 0
    EXE :=
endif

BIN := bitf$(EXE)

.PHONY: all clean format check-format install

all: $(BIN) bundle

$(BIN): $(OBJ)
	$(CXX) $(CXXFLAGS) $(OBJ) -o $@ $(LDFLAGS)
$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

$(BUILD_DIR)/%.o: %.cpp | $(BUILD_DIR)
	mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) -c $< -o $@

PREFIX ?= /usr/local
BINDIR = $(DESTDIR)$(PREFIX)/bin

install: bitf
	mkdir -p $(BINDIR)
	cp bitf $(BINDIR)/bitf
	chmod 755 $(BINDIR)/bitf

uninstall:
	rm -f $(BINDIR)/bitf 

clean:
	rm -rf $(BUILD_DIR) $(BIN)
ifeq ($(WINDOWS),1)
	@echo "Removing bundled DLLs..."
	@rm -f *.dll 2>/dev/null || true
endif

ifeq ($(WINDOWS),1)
bundle: $(BIN)
	@echo "Bundling MinGW DLLs..."
	ldd $(BIN) | grep "$(MINGW_BIN)" | awk '{print $$3}' | xargs -I{} cp -u {} .
else
bundle:
endif

format:
	clang-format -i $(SRC) Utilities/*.hpp