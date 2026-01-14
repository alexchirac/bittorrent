CC = mpic++
CFLAGS = -g -pthread -Wall -std=c++11
LDFLAGS = -lmpi

BIN_DIR = ../bin
SRC_DIR = .
TARGET = $(BIN_DIR)/bitnet

SOURCES = $(SRC_DIR)/bitnet.cpp
OBJECTS = $(SOURCES:.cpp=.o)

.PHONY: all clean build

all: build

build: $(TARGET)

$(TARGET): $(OBJECTS) | $(BIN_DIR)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.cpp
	$(CC) $(CFLAGS) -c $< -o $@

$(BIN_DIR):
	mkdir -p $(BIN_DIR)

clean:
	rm -f $(OBJECTS) $(TARGET)
	rmdir $(BIN_DIR) 2>/dev/null || true

rebuild: clean build
