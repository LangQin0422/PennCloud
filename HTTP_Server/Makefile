CC = g++

TARGETS = libHTTP_Server.so test_master test_worker install

TARGETS_LIB = libHTTP_Server.so

CFLAGS = -Wall -g -w -I./include -fPIC

OBJS = server.o requestImp.o responseImp.o helper.o test_client

LDFLAGS = -lresolv

BUILD_DIR = ./build

SRC_DIR = ./src

INCLUDE_DIR = ./include

DATA_DIR = ./data

all: $(TARGETS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CC) $(CFLAGS)  -lpthread -c $< -o $@

test_worker: $(addprefix $(BUILD_DIR)/, server.o helper.o test_worker.o)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lcurl

test_master: $(addprefix $(BUILD_DIR)/, server.o helper.o test_master.o)
	$(CC) $(CFLAGS) -o $@ $^ -lpthread -lcurl

libHTTP_Server.so: $(addprefix $(BUILD_DIR)/, server.o helper.o)
	g++ -shared -o $@ $^

install: $(TARGETS_LIB)
	sudo cp $(TARGETS_LIB) /usr/local/lib/
	sudo cp include/*.hh /usr/local/include/
	sudo ln -sf /usr/local/lib/$(TARGETS_LIB) /usr/lib/$(TARGETS_LIB)

test:
	g++ -o test_client ./src/test_client.cc -lcurl

clean::
	rm -fv $(TARGETS) $(OBJS) $(BUILD_DIR)/* *~ -r

$(shell mkdir -p $(BUILD_DIR))


# g++ -Wall -g -w -I./include -fPIC  -lpthread -c src/server.cc -o build/server.o
# g++ -Wall -g -w -I./include -fPIC  -lpthread -c src/helper.cc -o build/helper.o
# g++ -shared -o libHTTP_Server.so build/server.o build/helper.o