.PHONY: all server client clean light-clean





SERVER_PATH := ./server_src
CLIENT_PATH := ./client_src
COMMON_PATH := ./common_src


SERVER_HEADERS := server_headers.h
SERVER_SRCS := server.c database.c logger.c error_handler.c

CLIENT_HEADERS := client_headers.h
CLIENT_SRCS := client.c

COMMON_HEADERS := common_headers.h
COMMON_SRCS := utility.c


LINKER_OPT := -fPIE -std=gnu11
COMPILER_OPT := -fPIE -std=gnu11 -O3



COMMON_FULL_HEADERS := $(addprefix $(COMMON_PATH)/,$(COMMON_HEADERS))
COMMON_FULL_SRCS := $(addprefix $(COMMON_PATH)/,$(COMMON_SRCS))
SERVER_FULL_HEADERS := $(addprefix $(SERVER_PATH)/,$(SERVER_HEADERS)) $(COMMON_FULL_HEADERS)
SERVER_FULL_SRCS := $(addprefix $(SERVER_PATH)/,$(SERVER_SRCS)) $(COMMON_FULL_SRCS)
CLIENT_FULL_HEADERS := $(addprefix $(CLIENT_PATH)/,$(CLIENT_HEADERS)) $(COMMON_FULL_HEADERS)
CLIENT_FULL_SRCS := $(addprefix $(CLIENT_PATH)/,$(CLIENT_SRCS)) $(COMMON_FULL_SRCS)

SERVER_OBJS := $(SERVER_FULL_SRCS:.c=_server.o)
CLIENT_OBJS := $(CLIENT_FULL_SRCS:.c=_client.o)



all: server client light-clean

server: $(SERVER_OBJS)
	$(CC) $(SERVER_OBJS) -o $@ $(LINKER_OPT) -pthread -lcrypt
	
client: $(CLIENT_OBJS)
	$(CC) $(CLIENT_OBJS) -o $@ $(LINKER_OPT) -lcrypt

$(SERVER_OBJS): $(SERVER_FULL_SRCS) $(SERVER_FULL_HEADERS)
	$(CC) $(@:_server.o=.c) -c -o $@ $(COMPILER_OPT) -DSERVER

$(CLIENT_OBJS): $(CLIENT_FULL_SRCS) $(CLIENT_FULL_HEADERS)
	$(CC) $(@:_client.o=.c) -c -o $@ $(COMPILER_OPT) -DCLIENT

clean:
	rm -f server client $(CLIENT_OBJS) $(SERVER_OBJS) 

light-clean:
	rm -f $(CLIENT_OBJS) $(SERVER_OBJS) 
