CC = gcc
CFLAGS = -Wall -Wextra -O2 -std=c11

ifeq ($(OS),Windows_NT)
    EXE = .exe
    RM = del /Q
    RUN =
else
    EXE =
    RM = rm -f
    RUN = ./
endif

TARGET = retiming$(EXE)

SRCS = main.c retiming.c parser.c
OBJS = $(SRCS:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $(OBJS)

%.o: %.c retiming.h
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	-$(RM) $(OBJS) $(TARGET)

test: $(TARGET)
	$(RUN)$(TARGET) example_class.txt
	$(RUN)$(TARGET) 5_node_pipeline.txt
	$(RUN)$(TARGET) multi_loop.txt

.PHONY: all clean test