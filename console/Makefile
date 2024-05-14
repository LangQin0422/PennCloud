CC = g++

TARGETS = console

CFLAGS = -Wall -g -w -std=c++17 -I/usr/local/include -I$(INCLUDE_DIR)  # Added -std=c++17 here

LDFLAGS = -L/usr/local/lib -lHTTP_Server -pthread -lcurl

OBJS = $(BUILD_DIR)/console.o $(BUILD_DIR)/handler.o

DEPS = $(OBJS:.o=.d)

BUILD_DIR = ./build

SRC_DIR = ./src

INCLUDE_DIR = ./include

DATA_DIR = ./data

all: $(TARGETS)

-include $(DEPS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CC) $(CFLAGS) -MMD -c $< -o $@

console: $(addprefix $(BUILD_DIR)/, console.o handler.o)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean::
	rm -fv $(TARGETS) $(OBJS) $(BUILD_DIR)/* *~ -r

$(shell mkdir -p $(BUILD_DIR))

