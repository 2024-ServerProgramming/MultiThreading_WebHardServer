CC = gcc
CFLAGS = -Wall -O -g -Iinclude -pthread

SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

BIN = $(BINDIR)/server.exe

SRCS = $(SRCDIR)/server_main.c $(SRCDIR)/server_userAuth.c $(SRCDIR)/server_session.c
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
