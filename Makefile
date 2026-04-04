# FastPad Makefile
# Cross-compile for Windows using mingw-w64

CC = x86_64-w64-mingw32-gcc
CFLAGS = -Os -s -ffunction-sections -fdata-sections -Wall -Wextra
LDFLAGS = -Wl,--gc-sections -mwindows
LIBS = -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32

SRCDIR = src
BUILDDIR = build
TARGET = $(BUILDDIR)/FastPad.exe

SRCS = $(SRCDIR)/main.c \
       $(SRCDIR)/app.c \
       $(SRCDIR)/editor.c \
       $(SRCDIR)/buffer.c \
       $(SRCDIR)/fileio.c \
       $(SRCDIR)/render.c \
       $(SRCDIR)/search.c

all: $(TARGET)

$(TARGET): $(SRCS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
