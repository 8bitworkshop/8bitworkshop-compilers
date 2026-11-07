# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Repository Overview

This is the 8bitworkshop-compilers repository, which compiles retro computer toolchains to WebAssembly. The compilers support various 8-bit and 16-bit platforms and are used by the 8bitworkshop IDE.

Tools are being converted from Emscripten to WASI (wasi-sdk + wasmtime) — see AGENTS.md for the conversion playbook and per-tool notes. Currently on WASI: acme, dasm, yasm, vasm, zmac, naken_asm, c2t, makewav, merlin32, batariBasic, smlrc, cc2600, cc7800, nesfab, cc65, sdcc, xasm, cvbasic, gasm80. Everything else still uses Emscripten.

## Common Commands

### Building
- `make` - Build all compiler targets
- `make prepare` - Set up build directories and verify Emscripten
- `make clean` - Remove build artifacts
- `make clobber` - Clean everything including submodules
- `make <target>` - Build specific compiler (e.g., `make cc65`, `make sdcc`)

### Available Targets
cc65, xasm, sdcc, 6809tools, yasm, verilator, zmac, smlrc, nesasm, merlin32, batariBasic, c2t, makewav, fastbasic, dasm, Silice, wiz, armips, vasm, acme, tinycc, cvbasic, gasm80

### Prerequisites
- Emscripten SDK installed and activated
- Dependencies: `apt install libboost-graph-dev nasm bison flex texinfo zlib1g-dev`
- Git submodules initialized: `git submodule init && git submodule update`

## Architecture

### Build System
- **Main Makefile**: Orchestrates the entire build process with Emscripten
- **Two-phase builds**: Native builds first (for tools/generators), then WebAssembly builds
- **File packaging**: Uses Emscripten's file_packager to bundle filesystem data
- **Output structure**: 
  - `embuild/` - Temporary build directory
  - `output/wasm/` - Final WebAssembly modules
  - `output/fs/` - Filesystem data packages

### Compiler Integration Pattern
Each compiler follows a similar pattern:
1. **copy.TARGET** - Archives source from git submodule to build directory
2. **TARGET.libs** - Builds native tools/libraries needed for compilation
3. **TARGET.wasm** - Cross-compiles to WebAssembly with Emscripten
4. **TARGET.fsroot** - Prepares filesystem data (headers, libraries)
5. **Final target** - Combines WASM and filesystem packages

### Key Build Variables
- `EMCC_FLAGS` - Common Emscripten compilation flags
- `BUILDDIR` - Temporary build location (`embuild/`)
- `OUTPUTDIR` - Final output location (`output/`)
- `FILE_PACKAGER` - Emscripten tool for packaging filesystems

### Submodule Structure
All compiler source code is in git submodules, allowing version pinning and independent updates. The main repository contains only build configuration and integration logic.

### WebAssembly Integration
- Each compiler exports a specific module name (e.g., `EXPORT_NAME=cc65`)
- Filesystems are packaged separately for modularity
- Memory settings optimized for browser execution
- Some tools require special Emscripten flags (e.g., exception handling for C++)

## Development Notes

### Adding New Compilers
1. Add as git submodule
2. Create build target in Makefile following the established pattern
3. Handle any special build requirements (autotools, CMake, etc.)
4. Package required runtime files (headers, libraries)
5. Test WebAssembly output

### Debugging Build Issues
- Check Emscripten version compatibility
- Verify submodule versions with `git submodule status`
- Use `make prepare` to validate environment
- Build individual targets to isolate issues

### cc65 WASI tips (hard-won)

**Building** (see the `### cc65 (WASI)` section in the Makefile):
- `cc65/src/Makefile` honors `CC`, `AR`, `USER_CFLAGS`, `EXE_SUFFIX` overrides. Do **not** pass `CFLAGS` on the command line — its `+=` assignment ignores command-line overrides; put extra flags in `USER_CFLAGS` instead.
- `AR=$(WASI_SDK)/bin/llvm-ar` is required (host `ar` corrupts wasm object archives).
- The runtime libs (`lib/`, `target/` in the submodule) are **gitignored build artifacts**, not tracked files. They must be built fresh: first build native `ar65 ca65 cc65`, then `cd libsrc && PATH=<builddir>/cc65/bin:$PATH make -j4`. A stale checkout's old `*.lib` files predate the upstream `sp` → `c_sp` zeropage symbol rename and cause mysterious unresolved `c_sp` link errors.
- Native and wasm builds share the same `wrk/` object dir — run `make mostlyclean` between the native and wasm passes, or stale objects of the wrong type break linking (in either direction).
- Pin `BUILD_ID="Git <submodule short hash>"` on the wasm build. Without it, the binaries embed whatever `git rev-parse` finds — which walks up and picks the **outer** repo's hash, so goldens would change on every unrelated repo commit.
- WASI libc deprecates `getpid` (used by cc65's temp-filename logic in `common/fname.c`): compile with `-D_WASI_EMULATED_GETPID` and link with `-lwasi-emulated-getpid` (via `LDLIBS=...`) to silence the warning.

**Runtime data / filesystem packages**:
- cc65 bakes compile-time data dirs as absolute paths defaulting to `/share/cc65/{include,asminc,cfg,lib,target}`. So the fs zips must use that `share/cc65/...` layout; WASI resolves absolute paths against the preopened root, so no CLI flags needed at runtime.
- Per-platform fs packages (like the old Emscripten `fs65-<platform>.js/.data`): `cc65-fs-{none,nes,pce,atari2600,atari8,vic20,apple2,c64}.zip`, each with shared include/asminc + only that platform's cfg/lib/target data. Built from `$(BUILDDIR)/fs65-<platform>/fsroot` (single rule with a per-platform case statement; `cfg/atari*` would wrongly match atari2600/5200/7800, so atari8 uses explicit globs). The monolithic `cc65-fs.zip` (all platforms, ~21MB) is still built for tests/tooling.
- `test.cc65` uses `cc65-fs-nes.zip` (~800KB) to prove the per-platform packages work end-to-end.

**Running / testing**:
- Invoke as `wasmtime --dir=.::/ cc65.wasm ...` — the `HOST::GUEST` syntax maps the cwd at `/` so the baked-in `/share/...` paths resolve.
- Pipeline: `cc65 -t nes -T -o test.s test.c` → `ca65 -o test.o test.s` → `ld65 -t nes -o test.nes test.o /share/cc65/lib/nes.lib`.
- `make test.cc65` runs the golden-file test (`tests/cc65/`); goldens include the BUILD_ID string, so they must be regenerated whenever the submodule hash changes.