# DoS Testing Tool - C Version Makefile
# Supports Linux/macOS (gcc) and Windows (MinGW/MSYS2)

CC      = gcc
CFLAGS  = -Wall -Wextra -O2 -std=c11
LDFLAGS = -lpthread

# Windows-specific flags (MinGW)
ifeq ($(OS),Windows_NT)
    LDFLAGS += -lws2_32
    TARGET   = dos_tool.exe
else
    TARGET   = dos_tool
endif

SRCDIR   = .
ATKDIR   = attacks

SRCS = $(SRCDIR)/cli.c \
       $(ATKDIR)/http_flood.c \
       $(ATKDIR)/slowloris.c \
       $(ATKDIR)/tcp_syn.c \
       $(ATKDIR)/udp_flood.c

OBJS = $(SRCS:.c=.o)

.PHONY: all clean

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Header dependencies
$(SRCDIR)/cli.o: $(SRCDIR)/cli.c $(SRCDIR)/config.h \
                 $(ATKDIR)/http_flood.h $(ATKDIR)/slowloris.h \
                 $(ATKDIR)/tcp_syn.h $(ATKDIR)/udp_flood.h

$(ATKDIR)/http_flood.o: $(ATKDIR)/http_flood.c $(ATKDIR)/http_flood.h $(SRCDIR)/config.h
$(ATKDIR)/slowloris.o:  $(ATKDIR)/slowloris.c $(ATKDIR)/slowloris.h $(SRCDIR)/config.h
$(ATKDIR)/tcp_syn.o:    $(ATKDIR)/tcp_syn.c $(ATKDIR)/tcp_syn.h $(SRCDIR)/config.h
$(ATKDIR)/udp_flood.o:  $(ATKDIR)/udp_flood.c $(ATKDIR)/udp_flood.h $(SRCDIR)/config.h

clean:
ifeq ($(OS),Windows_NT)
	del /Q $(subst /,\,$(OBJS)) $(TARGET) 2>NUL || true
else
	rm -f $(OBJS) $(TARGET)
endif
