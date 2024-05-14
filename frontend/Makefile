CC = g++

TARGETS = frontend

CFLAGS = -Wall -g -w -std=c++17 -fPIC -I/usr/local/include -I/usr/lib -I/include -I/lib -I$(INCLUDE_DIR)

LDFLAGS = -L/usr/local/lib -lHTTP_Server -pthread -lcurl 

OBJS = $(BUILD_DIR)/frontend.o

DEPS = $(OBJS:.o=.d)

BUILD_DIR = ./build

SRC_DIR = ./src

INCLUDE_DIR = ./include

DATA_DIR = ./data

all: $(TARGETS)

-include $(DEPS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CC) $(CFLAGS) $(LDFLAGS) -MMD -c $< -o $@

frontend: $(OBJS)
	$(CC) -o $@ $^ $(CFLAGS) $(LDFLAGS)

clean::
	rm -fv $(TARGETS) $(OBJS) $(BUILD_DIR)/* *~ -r

$(shell mkdir -p $(BUILD_DIR))


# g++ -o frontend build/frontend.o -Wall -g -w -I/usr/local/include -lresolv -L/usr/local/lib -lHTTP_Server
