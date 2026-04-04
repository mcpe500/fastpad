# FastPad Makefile
# Cross-compile for Windows using llvm-mingw (Termux)

CC = x86_64-w64-mingw32-clang
CFLAGS = -Os -s -ffunction-sections -fdata-sections -Wall -Wextra
LDFLAGS = -Wl,--gc-sections -mwindows
LIBS = -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32 -lcomctl32

SRCDIR = src
BUILDDIR = build
TARGET = $(BUILDDIR)/FastPad.exe

SRCS = $(SRCDIR)/main.c \
       $(SRCDIR)/app.c \
       $(SRCDIR)/editor.c \
       $(SRCDIR)/buffer.c \
       $(SRCDIR)/fileio.c \
       $(SRCDIR)/render.c \
       $(SRCDIR)/search.c \
       $(SRCDIR)/tab_manager.c

all: $(TARGET)

$(TARGET): $(SRCS) | $(BUILDDIR)
	$(CC) $(CFLAGS) $(LDFLAGS) $^ -o $@ $(LIBS)

$(BUILDDIR):
	mkdir -p $(BUILDDIR)

clean:
	rm -rf $(BUILDDIR)

.PHONY: all clean
