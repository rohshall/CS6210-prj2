CC 	= gcc
CFLAGS	= -pedantic -Wall -std=gnu99 #-pthread
LDFLAGS = #-pthread

BINDIR	= bin

SERVER_TGT = $(BINDIR)/server
SERVER_SRCS = server.c
SERVER_OBJS = $(SERVER_SRCS:%.c=%.o)
SERVER_DEPS = $(SERVER_SRCS:%.c=%.d)

CLIENT_TGT = $(BINDIR)/client
CLIENT_SRCS = client.c
CLIENT_OBJS = $(CLIENT_SRCS:%.c=%.o)
CLIENT_DEPS = $(CLIENT_SRCS:%.c=%.d)

TGTS = $(SERVER_TGT) $(CLIENT_TGT)
SRCS = $(SERVER_SRCS) $(CLIENT_SRCS)
OBJS = $(SERVER_OBJS) $(CLIENT_OBJS)
DEPS = $(SERVER_DEPS) $(CLIENT_DEPS)

# Generic rules:
LINK	= $(LINK.c) -o $@ $^
COMP	= $(COMPILE.c) $<

all: $(TGTS)
$(SERVER_TGT): $(SERVER_OBJS)
	@mkdir -p $(BINDIR)
	$(LINK)
$(CLIENT_TGT): $(CLIENT_OBJS)
	mkdir -p $(BINDIR)
	$(LINK)
%.o: %.c
	$(CC) $(CFLAGS) -MMD -MP -c $<
	# $(COMP)
-include $(DEPS)

clean:
	rm -rf $(OBJS) $(DEPS) $(TGTS) $(BINDIR)

.PHONY: all clean
