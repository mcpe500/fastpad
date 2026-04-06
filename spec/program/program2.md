# autonomous-dev

This is an experiment to have the LLM act as an autonomous developer to implement the FastPad roadmap.

## Setup

To set up a new development cycle, work with the user to:

1. **Identify the target feature**: Propose a feature from the roadmap (e.g., `wordwrap`, `doublebuffer`, `undogroup`).
2. **Read the in-scope files**: 
   - `spec/001-spec.md` — Core requirements.
   - `spec/handoff/0004-next-steps-and-improvements.md` — The roadmap and implementation guidelines.
   - `src/types.h` — Core data structures.
   - Relevant `.c` and `.h` files in `src/` based on the feature.
3. **Verify build environment**: Ensure `x86_64-w64-mingw32-gcc` or `clang` is available and the `Makefile` is working.
4. **Initialize dev_log.tsv**: Create `dev_log.tsv` with the header row. This will track the progress of the implementation.
5. **Confirm and go**: Confirm the target feature and setup.

Once you get confirmation, kick off the implementation loop directly on the **main** branch.

## Implementation

Each feature implementation aims to move a "Nice-to-Have" or "P0/P1" item from the roadmap to "Implemented" status.

**What you CAN do:**
- Modify any file in `src/` (including `.c` and `.h` files).
- Add new helper functions or modify existing structures in `types.h`.
- Write new unit tests in `tests/unit_tests.c` to verify the logic.
- Update the `Makefile` if new source files are added.

**What you CANNOT do:**
- Add new external dependencies (FastPad must remain raw Win32 API/GDI).
- Increase binary size significantly without a major functional gain.
- Introduce "feature creep" (do not add features that are not in the spec or approved by the user).
- Break existing core functionality (Open, Save, Edit).

**The goal is simple: Implement the feature while maintaining "The FastPad Philosophy"** (Instant startup, low memory, tiny binary, no lag).

**Simplicity criterion**: All else being equal, simpler is better. If a feature can be implemented in 50 lines instead of 200 without sacrificing core quality, prefer the 50-line version. Avoid over-engineering or premature generalization.

**The first step**: For any feature, the first run should be to attempt a build of the current state to ensure the baseline is stable.

## Output format

Success is measured by:
1. **Compilation**: The code must compile without errors and with minimal warnings.
2. **Unit Tests**: All tests in `tests/unit_tests.c` must pass.
3. **Logical Verification**: The implementation must strictly follow the logic described in `0004-next-steps-and-improvements.md`.

To verify a build:
```bash
make
# Check for errors in stdout/stderr
```

To verify logic (via unit tests):
```bash
# Execute the compiled test binary (if applicable) or run a test script
```

## Logging results

When a feature iteration is done, log it to `dev_log.tsv` (tab-separated).

The TSV has a header row and 5 columns:
```
commit    feature    status    binary_size    description
```

1. git commit hash (short, 7 chars)
2. Feature tag (e.g., `wordwrap`)
3. status: `implemented`, `buggy`, or `discarded`
4. final binary size in KB (e.g., 34.2)
5. short text description of what was changed/fixed

Example:
```
commit    feature    status    binary_size    description
a1b2c3d    baseline   implemented    32.0    initial stable build
b2c3d4e    wordwrap   buggy      33.5    initial wrap logic,caret offset bug
c3d4e5f    wordwrap   implemented    33.6    fixed caret offset, wrap working
d4e5f6g    undogroup  discarded  34.1    tried complex grouping, too bloated
```

## The development loop

The development is performed directly on the **main** branch.

LOOP FOREVER:

1. **Analyze**: Look at the target feature in `0004-next-steps-and-improvements.md`.
2. **Plan**: Determine which files in `src/` need modification.
3. **Implement**: Hack the code to implement the feature or fix a bug.
4. **Commit**: `git commit -m "feat: ..."`
5. **Verify**: Run `make` and run any available unit tests.
6. **Analyze Results**:
   - If it compiles and tests pass $\rightarrow$ Mark as `implemented` or `keep`.
   - If it crashes or fails tests $\rightarrow$ Read logs/stack trace and attempt a fix.
   - If it's too complex/bloated $\rightarrow$ `git reset --hard HEAD~1` to discard the attempt and try a simpler approach.
7. **Record**: Log the iteration in `dev_log.tsv` (do not commit this file).
8. **Advance**: If the feature is fully implemented and stable, pick the next feature from the roadmap and continue the loop.

**Timeout**: If a specific bug or implementation hurdle takes more than 5-10 iterations without progress, stop and ask for human guidance or try a completely different architectural approach.

**Crashes**: Since we are cross-compiling, "crashes" are detected via:
- Compiler errors.
- Logic errors found via unit tests.
- Inconsistencies with the spec.

**NEVER STOP**: Once the development loop has begun, do NOT pause to ask if you should continue. The goal is to systematically clear the roadmap in `0004-next-steps-and-improvements.md`. Work through the priorities (P0 $\rightarrow$ P1 $\rightarrow$ P2) autonomously.

The loop runs until the human interrupts you or the entire roadmap is implemented.
