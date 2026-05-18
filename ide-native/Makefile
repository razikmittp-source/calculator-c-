CXX ?= g++
PKG_CONFIG ?= pkg-config
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wno-unused-parameter
LDFLAGS ?=
GTK_CFLAGS := $(shell $(PKG_CONFIG) --cflags gtk+-3.0)
GTK_LIBS := $(shell $(PKG_CONFIG) --libs gtk+-3.0)

BIN := abyss
SRC := abyss.cpp

all: $(BIN)

$(BIN): $(SRC)
	$(CXX) $(CXXFLAGS) $(GTK_CFLAGS) -pthread $(SRC) -o $(BIN) $(GTK_LIBS) $(LDFLAGS)

clean:
	rm -f $(BIN)

install: $(BIN)
	install -d $(DESTDIR)$(PREFIX)/bin
	install -m 0755 $(BIN) $(DESTDIR)$(PREFIX)/bin/abyss

.PHONY: all clean install
