CC = gcc
CFLAGS = -Wall -O -g -Iinclude

SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

BIN = $(BINDIR)/server_app

SRCS = $(SRCDIR)/main_server.c $(SRCDIR)/userAuth_server.c $(SRCDIR)/session_server.c
OBJS = $(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

# Default target
all: $(BIN)

# Compile binary
$(BIN): $(OBJS)
	@mkdir -p $(BINDIR)
	$(CC) $(CFLAGS) $(OBJS) -o $(BIN)

# Compile object files
$(OBJDIR)/%.o: $(SRCDIR)/%.c
	@mkdir -p $(OBJDIR)
	$(CC) $(CFLAGS) -c $< -o $@
	
clean:
	rm -rf $(OBJDIR) $(BINDIR)
