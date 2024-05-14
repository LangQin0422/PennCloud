CC = g++

TARGETS = echoserver smtp pop3 forward_server

CFLAGS = -Wall -g -w -I./include

OBJS = smpt_msg.o io_helper.o smpt_command.o pop3_msg.o pop3_command.o forward_server.o MX_helper.o

LDFLAGS = -lresolv -lHTTP_Server -L/usr/local/lib 

BUILD_DIR = ./build

SRC_DIR = ./src

INCLUDE_DIR = ./include

DATA_DIR = ./data

all: $(TARGETS)

$(BUILD_DIR)/%.o: $(SRC_DIR)/%.cc
	$(CC) $(CFLAGS)  -lpthread -c $< -o $@

echoserver: $(addprefix $(BUILD_DIR)/, echoserver.o)
	$(CC) $(CFLAGS) -pthread -o $@ $^ $(LDFLAGS)

smtp: $(addprefix $(BUILD_DIR)/, smtp.o smpt_msg.o io_helper.o smpt_command.o)
	$(CC) $(CFLAGS) -pthread -o $@ $^ $(LDFLAGS)

pop3: $(addprefix $(BUILD_DIR)/, pop3.o io_helper.o pop3_msg.o pop3_command.o)
	$(CC) $(CFLAGS) $^ -I/usr/local/opt/openssl/include -L/usr/local/opt/openssl/lib -lcrypto -lpthread -g -o $@ $(LDFLAGS)

forward_server: $(addprefix $(BUILD_DIR)/, forward_server.o io_helper.o MX_helper.o)
	$(CC) $(CFLAGS) -pthread -o $@ $^ $(LDFLAGS)

samples:
	mkdir mailtest
	touch mailtest/linhphan.mbox
	touch mailtest/bcpierce.mbox
	touch mailtest/zives.mbox
	touch mailtest/zxiao.mbox
	
pack:
	rm -f submit-hw2.zip
	zip -r submit-hw2.zip ./include/* ./src/* README Makefile

clean::
	rm -fv $(TARGETS) $(OBJS) $(BUILD_DIR)/* makeinput *~ -r

realclean:: clean
	rm -fv submit-hw2.zip

$(shell mkdir -p $(BUILD_DIR))
