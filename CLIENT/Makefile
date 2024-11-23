CC = gcc
CFLAGS = -Wall -O -g -Iinclude

SRCDIR = src
INCDIR = include
OBJDIR = obj
BINDIR = bin

BIN = ./$(BINDIR)/clint.exe

SRCS = ./$(SRCDIR)/client_main.c $(SRCDIR)/client_userAuth.c
OBJS = ./$(SRCS:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

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
