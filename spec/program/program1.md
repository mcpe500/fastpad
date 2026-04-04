# FastPad Program

This is a small native Windows text editor project.

The mission is to build a **tiny, fast, native plain-text editor** that feels better than bloated editors because it is simpler.

## Core Idea

Build a Win32 text editor in C.

Do not build a mini browser.
Do not build an IDE.
Do not build a framework.

The output should be a straightforward Windows `.exe` that opens quickly, edits text reliably, and stays small.

## Setup

When starting a new implementation run, do the following:

1. **Read the in-scope files**
   - `spec.md` — product and technical requirements
   - `program.md` — implementation rules
   - any existing files under `src/`

2. **Understand the target**
   - native Windows GUI app
   - raw Win32 API
   - C source
   - no large dependencies

3. **Confirm project shape**
   - keep source code small
   - prefer a few `.c` and `.h` files
   - avoid introducing unnecessary layers

4. **Prepare the build**
   - if on Linux, use `mingw-w64`
   - if on Windows, use MSVC or clang
   - build a single `FastPad.exe`

## Hard Constraints

### You CAN
- write and modify C code
- use raw Win32 API
- use GDI
- add small helper modules
- improve architecture if it remains simple

### You CANNOT
- use Electron
- use Chromium
- add .NET / Java / browser-runtime dependence
- add giant UI frameworks
- turn this into a plugin platform
- bloat the app for low-value features

## Product Priorities

When deciding what to do next, use this order:

1. correctness
2. responsiveness
3. simplicity
4. low memory
5. binary size
6. convenience features

If a change improves a lower-priority item but hurts a higher-priority one, reject it.

## v1 Must-Haves

The first usable version must support:

- create/edit plain text
- open
- save
- save as
- text insertion/deletion
- caret movement
- selection
- copy/cut/paste
- undo/redo
- find
- status bar
- unsaved-changes prompt

Do not chase extras before these work well.

## Recommended Implementation Order

Implement in this order:

1. **Window shell**
   - WinMain
   - register window class
   - create main window
   - message loop
   - menu bar
   - status bar placeholder

2. **Minimal editor model**
   - text buffer
   - caret position
   - modified flag
   - current file path

3. **Basic rendering**
   - choose monospace font
   - draw visible text
   - show caret
   - handle resize

4. **Basic editing**
   - character input
   - Enter
   - Backspace
   - Delete
   - arrow keys
   - mouse click for caret placement

5. **Selection and clipboard**
   - drag select
   - copy
   - cut
   - paste

6. **File I/O**
   - open dialog
   - save
   - save as
   - UTF-8 handling
   - unsaved-changes prompts

7. **Undo/redo**
   - implement simple bounded history
   - keep design understandable

8. **Find**
   - simple find dialog
   - find next

9. **Polish**
   - status bar line/column
   - word wrap toggle if simple
   - reduce flicker
   - tighten memory behavior

## Architectural Rules

### Keep modules boring
Use straightforward modules such as:

- `main.c`
- `app.c`
- `editor.c`
- `buffer.c`
- `render.c`
- `fileio.c`
- `search.c`

### Keep state understandable
Do not scatter critical state randomly.
Do not create a deeply abstract object system.
Use simple structs.

### Prefer direct code
A few clear functions are better than a “generic engine.”

## Buffer Strategy

Preferred:
- start with a **gap buffer**

Why:
- simple
- efficient enough for local editing around the caret
- easy to reason about

If the gap buffer becomes ugly or blocks clean undo/redo behavior, a piece table may be considered.

Do not jump to ropes or complex structures unless there is a real problem.

## Rendering Strategy

For v1:
- use GDI
- render visible lines only
- avoid fancy effects
- use invalidation conservatively

Typing responsiveness matters more than visual sophistication.

## Memory Strategy

The target is **small**, not magical.

Do not promise impossible numbers like “always under 1 MB total RAM.”
Instead do the real work:

- avoid background threads
- avoid giant caches
- avoid loading unnecessary resources at startup
- allocate on demand
- free what is no longer needed
- keep data structures compact

The best memory optimization is not creating junk.

## UX Strategy

The app should feel good because it is predictable.

That means:

- instant launch
- simple menus
- clean shortcuts
- no surprise background activity
- no cluttered UI
- no frozen input during normal use

If a feature complicates the mental model, it is probably wrong for FastPad.

## Definition of Better

“Better” does NOT mean “more like a full IDE.”

“Better” means:
- faster launch
- less lag
- less clutter
- easier to understand
- simpler behavior
- fewer ways to break

## Build Rules

### Linux to Windows cross-build

Install toolchain:

```sh
sudo apt update
sudo apt install -y gcc-mingw-w64-x86-64 make
````

Build:

```sh
mkdir -p build
x86_64-w64-mingw32-gcc \
  -Os -s \
  -ffunction-sections -fdata-sections \
  -Wl,--gc-sections \
  -mwindows \
  src/main.c src/app.c src/editor.c src/buffer.c src/fileio.c src/render.c src/search.c \
  -o build/FastPad.exe \
  -luser32 -lgdi32 -lcomdlg32 -lkernel32 -lshell32
```

Optional local test on Linux with Wine:

```sh
wine build/FastPad.exe
```

### Native Windows build

Equivalent builds with MSVC or clang are acceptable as long as the result stays a small native `.exe`.

## Repo Rules

If this project is being implemented by an automated coding agent:

1. read `spec.md` first
2. make the smallest useful change
3. keep the code compiling
4. prefer completion of core features over speculative cleanup
5. do not add dependencies just to save a little coding time
6. do not rewrite working code for style reasons alone

## Decision Rule for New Features

Before adding a feature, ask:

1. does this help core text editing?
2. does this keep the app small?
3. does this keep the code understandable?
4. does this preserve responsiveness?

If the answer is not clearly yes, skip it.

## Quality Bar

A change is good if it does one or more of these:

* fixes correctness
* improves responsiveness
* reduces complexity
* reduces memory use
* reduces binary size
* improves a core workflow without adding much code

A change is bad if it:

* adds complexity without strong benefit
* expands scope away from plain text editing
* introduces framework-like abstractions
* makes the app feel heavier

## v1 Stop Condition

v1 is done when:

* open/save/edit works
* selection and clipboard work
* undo/redo works
* find works
* the app feels responsive
* the code is still small and sane
* a single Windows `.exe` is produced cleanly

## Final Rule

Always choose the simpler path that produces a solid native editor.

FastPad should feel like a tool, not a platform.

