# cshell/Makefile - Build system for the shell

CC = gcc
CFLAGS = -Wall -Wextra -g -I./include -std=c11
LDFLAGS = 
TARGET = cshell

SRCDIR = src
OBJDIR = obj

# All source files to compile
SOURCES = $(SRCDIR)/main.c $(SRCDIR)/parser.c $(SRCDIR)/executor.c $(SRCDIR)/builtins.c $(SRCDIR)/jobs.c
OBJECTS = $(SOURCES:$(SRCDIR)/%.c=$(OBJDIR)/%.o)

all: $(TARGET)

$(TARGET): $(OBJDIR) $(OBJECTS)
	$(CC) $(CFLAGS) -o $@ $(OBJECTS) $(LDFLAGS)

$(OBJDIR):
	mkdir -p $(OBJDIR)

$(OBJDIR)/main.o: $(SRCDIR)/main.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/parser.o: $(SRCDIR)/parser.c include/parser.h include/shell.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/executor.o: $(SRCDIR)/executor.c include/executor.h include/shell.h include/builtins.h include/jobs.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/builtins.o: $(SRCDIR)/builtins.c include/builtins.h include/shell.h include/jobs.h
	$(CC) $(CFLAGS) -c -o $@ $<

$(OBJDIR)/jobs.o: $(SRCDIR)/jobs.c include/jobs.h include/shell.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(OBJDIR) $(TARGET)

rebuild: clean all

run: $(TARGET)
	./$(TARGET)

.PHONY: all clean rebuild run