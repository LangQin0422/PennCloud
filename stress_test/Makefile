CC = g++

TARGETS = stress_test

CFLAGS = -Wall -g -w -I./include -fPIC

LDFLAGS = -lpthread -lcurl -lresolv

BUILD_DIR = ./build

SRC_DIR = ./src

all: $(TARGETS)

stress_test:
	 g++ -o stress_test stress_test.cc $(CFLAGS) $(LDFLAGS) 

clean::
	rm -fv $(TARGETS) $(OBJS) $(BUILD_DIR)/* *~ -r


# g++ -Wall -g -w -I./include -fPIC  -lpthread -c src/server.cc -o build/server.o
# g++ -Wall -g -w -I./include -fPIC  -lpthread -c src/helper.cc -o build/helper.o
# g++ -shared -o libHTTP_Server.so build/server.o build/helper.o