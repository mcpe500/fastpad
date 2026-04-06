# FastPad Agent Instructions (AGENTS.md)

This document serves as the primary context and instruction set for any LLM agent working on FastPad.

## Project Essence
FastPad is a "tiny, fast, native Windows text editor". 
**The Golden Rule**: "Better means: faster launch, less lag, less clutter, easier to understand, simpler behavior, fewer ways to break."

## Current Architecture
- **Language**: C / Win32 API / GDI.
- **Text Storage**: Gap Buffer (`src/buffer.c`).
- **Rendering**: Double-buffered GDI rendering with a line-number margin and word-wrap support (`src/render.c`).
- **Undo/Redo**: Bounded stack (1000 ops) with grouping for consecutive edits (`src/editor.c`).
- **Multi-tab**: Basic tab management via `SysTabControl32` (`src/tab_manager.c`).

## Core Implementation Rules
1. **No Bloat**: Do not add external dependencies. Use raw Win32 API.
2. **Memory Efficiency**: Allocate on demand, free immediately. Avoid caches unless they provide a massive performance boost.
3. **Simplicity**: Prefer a 50-line simple solution over a 200-line "extensible" one.
4. **Binary Size**: Target executable size is < 100 KB.

## Roadmap & State
- [x] Core Editing (Insert, Delete, Navigation)
- [x] File I/O (UTF-8, BOM)
- [x] Undo/Redo with Grouping
- [x] Word Wrap (Visual mapping)
- [x] Double Buffering (Flicker-free)
- [x] Line Number Margin
- [x] Persistent Undo History (Saved to %TEMP%)
- [x] Find/Find Previous Dialog

## Development Workflow
1. **Branching**: Work directly on `main` for fast iterations.
2. **Verification**: 
   - Run `make` to ensure zero errors.
   - Verify binary size with `ls -lh build/FastPad.exe`.
   - Log every significant change in `dev_log.tsv`.
3. **Commit**: Use conventional commits (e.g., `feat: ...`, `fix: ...`).
