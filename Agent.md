# Project Rules — toml-parser-engine

## 1. Memory: arena allocation only
- All memory must come from the arena allocator API. Manual `malloc` / `free` (and any other raw allocation) is forbidden.
- One **father arena** is created by an init handler at project startup. It is the single root of memory ownership.
- From the father arena, carve **child arena spaces** dedicated to different uses (lexer, parser, AST, io, tests, ...).
- Allocate with `arena_alloc` only. Free nothing by hand: child arenas are released into the father, and everything dies when the father arena is destroyed at shutdown.

## 2. Language: pure C
- Core project code must be pure C: no classes, STL, templates, exceptions, RAII, `new`/`delete`, or other C++ features.
- Source files use the `.cpp` extension only for easy compiling/linking in one test binary with gtest. The extension does not sanction C++ features — the code inside is C.
- The only C++ allowed in the whole project is the test code in the `test/` folder (gtest requires it).

## 3. Tests (gtest)
- Every function written for the project must have tests.
- Tests live **only** in the `test/` folder and use the **gtest** library; C++ features are permitted here and only here.

## 4. Cross-platform compatibility (Windows / Linux)
- Wherever Windows API and Linux/POSIX API differ, wrap them behind a C header with conditional compilation (`#ifdef _WIN32 ... #else ... #endif`) so the platform-specific implementation is hidden.
- public function signatures must be identical on every OS, and behavior/output must be the same on every OS. Callers never touch OS APIs directly.

## 5. Clean code
- Functions must stay within **32 lines**; split longer logic into smaller helper functions.
- Keep code readable and consistent with surrounding style.

## 6. Building and running tests
- The `Makefile` already contains the build and test commands; use it, do not invent new ones.
- On **Linux**: run tests from an Ubuntu / Unix terminal.
- On **Windows**: run tests through the **MSYS2 terminal** (inside VS Code), because all tools and libraries live there — never use the default Windows shell (PowerShell / cmd).

## 7. AI worker / reviewer workflow
- For any task, the reviewer (team lead) spawns **at least 3 parallel worker sessions** running **DeepSeek V4.1 Flash (`max`)** (`opencode/deepseek-v4.1-flash-free`) organized as a team; workers may themselves spawn sub-workers for independent sub-tasks. The lead gives each worker its slice of the task plus these rules, and hands the task (with rules as context) to the team.
- The reviewer agent must then audit the worker's output before anything is delivered:
  - hunt for undefined behavior in memory: buffer/integer overflow, alignment problems, use-after-arena, out-of-bounds access;
  - verify OS error handling (Windows / Linux handlers) inside the `#ifdef` wrappers only;
  - verify compliance with rules 1–6 of this file;
  - review the tests, hunt for edge cases that would make them fail, write new tests, and fix failures.
- Workers may themselves spawn sub-workers: each worker then acts as a **lead** who must review its own sub-agents' code, fix any problems it finds, and submit to the top reviewer only after its internal review. The top reviewer never relies on the worker's internal green light — it audits independently.
- Scoring per submitted deliverable: if it survives the top reviewer's audit with zero problems on the first pass, the worker earns **10 points**; if the top reviewer finds problems and a fix round is needed, the worker earns **5 points**.
- The reviewer applies fixes directly on the branch with its own edits, reviews the results of the whole worker team, and gives the user one consolidated lead report: what each worker did, the mistakes found, the fixes applied, and the verification (build, tests, sanitizers, both OSes). The user reviews only the lead's final code and report.
- The worker model's opinions or intermediate edits are never delivered to the user.
- After building, run the tests with **sanitizers** to catch memory bugs the compiler hides: AddressSanitizer for memory errors + UndefinedBehaviorSanitizer for UB (`-fsanitize=address,undefined` with gcc/clang in MSYS2 and on Linux), plus `-Wall -Wextra`. Every sanitizer report is treated as a bug to reproduce, fix, and re-verify with a clean run; if the sanitizer needs extra libraries that are missing, install them through MSYS2 packages (never build with sanitizers silently disabled).

## 8. C coding conventions
- The arena allocator returns `void*`; every caller must explicitly cast to the target type on the spot (e.g. `Token *t = (Token *)arena_alloc(...)`), never leave it implicit and never propagate bare `void*`.
- Standard C library file I/O is accepted in the project (`fopen`, `fread`, `fwrite`, `fclose`, ...): it is inherently consistent across Windows and Linux, so it satisfies rule 4. Raw Win32/POSIX file handles are still forbidden outside the `#ifdef` wrappers.
