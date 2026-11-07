# AGENTS.md — WASI Conversion Notes

Practical notes for converting the Emscripten-based compiler builds in this
repo to WASI (wasi-sdk). Complements CLAUDE.md.

## Status

- Converted to WASI: **acme**, **dasm**, **yasm**, **vasm**, **zmac**,
  **naken_asm**, **c2t**, **makewav**, **merlin32**, **batariBasic** (all 4
  pipeline tools), **smlrc** (with smlrc-fs.zip), **cc2600**, **cc7800**,
  **nesfab**, **cc65**, **macross** (6502) + **slinky**, **sdcc** (4.x: z80/z180/ez80, sm83 (Game Boy),
  mos6502/65c02), **xasm** (ASxxxx 1.50: as6800/6801/6804/6805/6809/6811/
  asz80/asi85, aslink, aslib), **verilator** (4.228), **cvbasic** (with
  cvbasic-fs.zip)
- acme/dasm/yasm/vasm/c2t/makewav need no FS package; wiz will when
  converted; sdcc ships `share/sdcc/{include,lib/<port>}` (sdcc-fs.zip); cc65 ships `share/cc65/{include,asminc,cfg,lib,target}` (monolithic
  `cc65-fs.zip` plus per-platform `cc65-fs-<platform>.zip`); cc2600/cc7800 ship
  headers/, nesfab ships lib/, cvbasic ships its prologue/epilogue .asm under
  share/cvbasic/
- Everything else still uses Emscripten (`emcc`/`emmake`/`emconfigure`)
- `make test` runs the WASI tool tests (see the `test:` aggregate in the
  Makefile for the current list, e.g. `test.cc65`)

## Toolchain

- wasi-sdk is expected at `~/wasi-sdk` (override with `WASI_SDK=...`);
  `prepare` fails fast if missing
- Key vars defined in the Makefile: `WASI_CC`, `WASI_STRIP`, `WASI_SYSROOT`,
  `WASI_CFLAGS` (`--sysroot=...`); wasi-sdk clang defaults to
  wasm32-wasip1, no `-target` needed
- The Emscripten smoke test lives in `check.emcc`, not `prepare`, so WASI
  builds don't require emcc (the Homebrew emcc also fails locally: unwritable
  cache + boost port download)

## Conversion pattern (per tool)

Replace the `### tool` section:

```make
### tool (WASI)

tool.wasi: copy.tool
	cd $(BUILDDIR)/tool/<builddir> && PATH="$(WASI_SDK)/bin:$$PATH" \
		make CC="$(WASI_CC) $(WASI_CFLAGS)" CFLAGS="-O2"

tool: tool.wasi
	cp $(BUILDDIR)/tool/<builddir>/tool.wasm $(WASMDIR)/tool.wasm
```

- No `EXPORT_NAME`, no JS glue, no `%.js` step — output is a standalone
  wasm in `output/wasm/`
- Prepend `$(WASI_SDK)/bin` to PATH so a Makefile's bare `strip` resolves
  to LLVM's wasm-aware strip (macOS `/usr/bin/strip` only warns)
- Prefer make-variable overrides (`CC=`, `CFLAGS=`) over `sed`-patching the
  upstream Makefile. If you must sed: BSD/GNU sed differences bite —
  `\t` doesn't work in BSD sed patterns, and `/` in `$(WASI_CC)` breaks
  `s///` delimiters
- If the tool has build-time generators (yasm's genmodule/genperf/etc.):
  configure with `--host=wasm32-wasi` so cross-compiling is detected and
  `CC_FOR_BUILD=cc` builds the generators natively — no separate native
  build needed. Keep `PATH="$(WASI_SDK)/bin:$$PATH"` on the make call so
  `ar`/`ranlib`/`strip` are the LLVM ones; host ar on wasm objects
  corrupts archives (leads to confusing `section too large` from wasm-ld)
- Always start from a clean copy (`copy.%` git archive) — reusing a build
  dir across configure attempts leaves stale objects in archives

## Filesystem packages

- Only for tools that ship runtime data (headers/libs): stage it in
  `$(BUILDDIR)/tool/fsroot`, and the generic rule zips it to
  `$(FSDIR)/tool-fs.zip`
- Naming matches the 8bitworkshop IDE convention
  (`src/worker/fs/<tool>-fs.zip`); the IDE unzips via JSZip into a
  `WASIMemoryFilesystem` with zip contents at the WASI root
  (see IDE `src/worker/wasiutils.ts` and `src/worker/tools/cc2600.ts`)
- acme/dasm need no FS package; wiz will when converted

## cc65 (WASI)

- The runtime 6502 libraries are build-time data for ld65, not wasm: build
  native `ar65 ca65 cc65 ld65` first and run `libsrc` (produces `lib/`,
  `target/`), then `mostlyclean` and rebuild `cc65 ca65 ld65` with the WASI CC.
- `libsrc` needs **all four** native tools, including `ld65` for the driver
  modules (`../bin/ld65*` wildcard in `libsrc/Makefile`). If you only build
  `ar65 ca65 cc65`, it silently falls back to a system `ld65` - and if a stale
  `bin/ld65.wasm` exists the wildcard resolves to a nonexistent native binary.
- `copy.cc65` removes the whole build dir before extracting the git archive;
  without that, stale `bin/*.wasm` from the previous (wasm) build poison the
  wildcard lookup.
- `BUILD_ID` is pinned to `N/A`. If left unset, cc65's Makefile runs git in
  the extracted copy, which walks up to this repo's `.git` and embeds *our*
  HEAD hash - the version banner changes every commit and breaks the golden.
- `$(BUILDDIR)/copy.cc65` and `$(BUILDDIR)/cc65.wasi` are real stamp files
  (in `embuild/`, removed by `make clean`): the extract/build only re-runs when the cc65 submodule HEAD
  or the Makefile changes, so a no-op `make test.cc65` is ~0.4s instead of
  ~60s. The other WASI tools are not stamped and rebuild every time.
- fs layout: `include/`+`asminc/` plus per-platform `cfg/`/`lib/`/`target/`
  at `/share/cc65/...`; the globs in `CC65_PLATFORMS` avoid cross-platform
  bleed (e.g. `cfg/atari*` would also match `atari2600.cfg`).

## sdcc (WASI)

- **Pinned to SDCC 4.6.0** release (`d79ee7d51`, 2026-06-12: RC2 fixes merged;
  `.version` = 4.6.0). The SourceForge git mirror has no tags, and trunk's
  `.version` (4.6.x) is just a dev counter - not releases. Check out that
  commit in `sdcc/` before `make sdcc`; `patches/sdcc-wasi.patch` applies to it.
- Two copies of the submodule: `$(BUILDDIR)/sdcc` is a full **native** build
  (needs native sdcpp/sdar etc.) that only exists to compile the device
  libraries (`device/lib/build/<port>/{crt0.rel,<port>.lib}`) and install the
  headers into `fsroot`; `$(BUILDDIR)/sdcc-wasi` is re-extracted, patched with
  `patches/sdcc-wasi.patch`, and cross-built with `--host=wasm32-wasi`.
  `$(BUILDDIR)/copy.sdcc`/`sdcc.native`/`sdcc.wasi` are stamps like cc65's.
- wasm outputs: `sdcc` (compiler proper), `sdasz80 sdasgb sdas6500`,
  `sdldz80 sdldgb sdld6808`, `makebin`. The four sdld* are one binary that
  picks its target from argv[0] (substring `z80`/`gb`/`6808`; plain `sdld`
  means 8051) - the host must pass a matching argv[0] (wasmtime uses the
  file name). mos6502/65c02 link with sdld6808.
- No subprocesses on WASI, so the `sdcc` driver can't run sdcpp/sdas/sdld:
  use `--c1mode` (reads **preprocessed** C from stdin, implies -S, `-o
  x.s`) and run the assembler/linker yourself. The patch makes
  system()/popen() print an error instead of failing to link. Preprocess
  elsewhere (mcpp, or a native sdcpp) - `# N "file.c"` markers matter: the
  `;file.c:N: <source>` asm comments come from sdcc re-reading that file.
- Patch details (`patches/sdcc-wasi.patch`): 3-arg `main(argc,argv,envp)` is
  not recognized as the wasm entry point (trap in `undefined_weak:main`) -
  shimmed with a 2-arg main passing `environ`; the port C preamble was fed
  via pipe() - now a scratch file `sdcc-preamble.tmp` in cwd (fmemopen
  doesn't work: the flex scanner is built with `-f` and read()s from
  fileno(yyin)); `dup2` for --use-stdout is skipped; `printCLine` guards
  `fopen(NULL)` (glibc/macOS return EFAULT, wasi-libc aborts) for input
  without line markers.
- EH: sdcc's C++ (Boost.Graph) never catches, so it is built
  `-fno-exceptions -DBOOST_NO_EXCEPTIONS` and linked against wasi-sdk's
  `noeh` libc++ (no `-fwasm-exceptions`/`-lunwind`); `patches/sdcc-noexc.cc`
  (-> `sdcc-noexc.a`) supplies `boost::throw_exception`. The eh/ libc++ is
  exnref-only and would need `--experimental-wasm-exnref` on Node < 25.
  sdas uses setjmp (err 'q' longjmps out of a line) -> `-mllvm -wasm-enable-sjlj
  -mllvm -wasm-use-legacy-eh=true -lsetjmp`: legacy try/catch validates on
  Node and all browsers without flags (wasmtime rejects it; not our host).
  Check with `llvm-objdump -d x.wasm | grep -c try_table` (sdcc must be 0).
- Flags (older notes): sdas uses setjmp ->
  `-mllvm -wasm-enable-sjlj -lsetjmp`. With wasi-sdk 33 `-mllvm` alone does
  not define `__wasm_exception_handling__` (setjmp.h #errors): add
  `-mexception-handling`, in CPPFLAGS too because dependency generation runs
  cpp without CFLAGS. sdas Makefiles hardcode `LIBS = -lm`, so link libs go
  in LDFLAGS (wasm-ld resolves archives order-independently).
- configure insists on zlib.h but only sdcpp/sdbinutils use it:
  `ac_cv_header_zlib_h=yes`. Boost graph headers via `-idirafter
  $(BOOST_INCLUDE)`. Old bundled config.sub -> `NEW_CONFIG_SUB`.
- `--prefix=/` makes the built-in search paths `/share/sdcc/include` and
  `/share/sdcc/lib/<port>`, matching the fs zip layout (cf. cc65).
  `--disable-non-free`: the non-free tree is PIC-only (124MB of headers).
- The sdas `.lst`/sdld `.map` files embed a timestamp; the golden test
  compares only `.s` and `.ihx` (z80, sm83, mos6502). The `.s` banner says
  `#0 (UNIX)` - no svn build number, so it is stable across builds.

## verilator (WASI)

- Source only needs **host** Python/flex/bison for the generated sources
  (`src/astgen`, `src/bisonpre`, `src/flexfix` are Python): `copy.verilator`,
  `autoconf`, then `./configure --host=wasm32-wasi --prefix=/share`, then
  `make -C src opt`. The old native `verilator.libs` and the emscripten
  `verilator.prepare`/`verilator.update` rules are gone.
- `patches/verilator-wasi.patch` (applied in `verilator.wasi`):
  - `src/Verilator.cpp`: 3-arg `main(argc,argv,env)`/... is not recognized by
    wasm-ld as the entry point (traps in `undefined_weak:main`); drop the
    unused `env` -> 2-arg main. (Upstream did this later.)
  - `src/V3Os.cpp`: guard `<sys/wait.h>` and stub `V3Os::system()`
    (`v3error` + `-1`) since WASI has no subprocesses; only affects
    `--build`/`--hierarchical`.
  - `include/verilatedos.h`: `VL_DEV_NULL` is `/dev/null` on Unix, which
    doesn't exist under WASI (lint-only opens it for output) -> scratch file
    `verilator_devnull.tmp` in cwd.
- `-fno-exceptions`: wasi-sdk's default libc++ is the **noeh** variant, so
  without this the libc++ headers emit `__cxa_throw`/`__cxa_allocate_exception`
  references that the noeh runtime doesn't define. Verilator itself has no
  throw/catch.
- `LDFLAGS=-Wl,-z,stack-size=1048576`: the default wasm stack is 64 KiB and
  `VInFilterImp::readBlocks` puts a 64 KiB buffer on the stack, so reads trap
  on out-of-bounds memory.
- flex doesn't ship `FlexLexer.h` with macOS's system flex (Apple's emits
  `size_t LexerInput/LexerOutput`, GNU's emits `int`); `verilator.wasi` copies
  the header matching the flex in use (`FLEXLEXER_H` in the Makefile) into
  `src/` before building.
- No FS package; `--lint-only` and `--cc` both run (the generated `obj_dir/`
  is relative to cwd, so preopen `--dir=.`).

## xasm (WASI)

- Not a submodule and not vendored: `$(BUILDDIR)/xasm.tar.gz` is downloaded
  from the Internet Archive (`XASM_URL`; original source was
  cher.media.mit.edu, 1997), verified against the pinned `XASM_SHA256`, and
  extracted by `xasm.wasi`. `make clean` removes the cached tarball, so a
  fresh build needs network access to web.archive.org.
- Old K&R C, built with `-std=gnu89 -w`. No source patches: a generated
  `xasm-wasi.h` is force-included (`-include`) and includes `<stdio.h>` first,
  then `#define getline xasm_getline` / `#define link xasm_link` (libc
  clashes) and declares `void machine(); void outdp();` - they are defined
  VOID but called via implicit int, and wasm-ld turns such a signature
  mismatch into a trapping stub (only a link *warning*, so watch for
  `function signature mismatch`).
- setjmp/longjmp: same wasm EH flags as merlin32. The upstream Makefiles
  ignore `LDFLAGS`/`LDLIBS`, so `-lsetjmp` is appended to `CC` itself.
- CLI of this aslink variant: `as6809 -l -o x.rel x.asm` (`-o` takes a
  filename), then `aslink -o x x.rel` -> `x.s19` (`-i` adds `x.ihx`).
  Control-file style (`-f`) is not what this build expects.
- Test goldens (`tests/xasm/*.expected`) are the S19 output; verified
  byte-identical to a native build of the same sources.

## Rust-based tools (cc2600 / cc7800)

- Both are cargo crates: just `cargo build --release --target wasm32-wasip1`
  (needs `rustup target add wasm32-wasip1`); no C toolchain needed
- cc7800 depends on a path crate `../cc6502` — the cc6502 submodule is
  copied to `$(BUILDDIR)/cc6502` (sibling of the cc7800 build dir) via
  `copy.cc6502`; cc2600 pulls cc6502 from crates.io instead
- First build fetches crates from crates.io (cargo registry cache needed)
- cc2600 is deterministic with `-S` (asm only); without it, it shells out
  to DASM at the end
- cc7800's asm output embeds the output *filename* in the cart title
  string (`17..48 header`) — goldens must be generated with the same -o name
- Both ship runtime headers: fs packages are `headers/` at zip root
  (cc2600 needs `-I headers`, cc7800 `-I headers -S` for asm-only output)

## nesfab (WASI)

- sehugg/nesfab fork has a built-in WASI target: `make ARCH=WASI wasi
  WASI_SDK_PATH=... OBJDIR=obj_wasi` — uses wasm exceptions
  (-fwasm-exceptions, needs modern wasmtime), `-DNO_THREAD` (its own
  single-thread fallback), and Homebrew boost headers via
  `BOOST_INCLUDE` (brew --prefix boost)
- nesfab needs `lib/` (the .fab standard library) at runtime →
  nesfab-fs.zip; invoke with `-I lib`
- Building inside the nesfab Makefile also emits a harmless error while
  generating dep files for its (unbuilt) test binary — make ignores it
  (-include) and the build succeeds
- hello_world example compiles deterministically to a .nes ROM (the
  "Mapper NROM requires chrrom" warning is normal)

## cvbasic (WASI)

- CVBasic is a BASIC cross-compiler emitting Z80 (Colecovision/SG-1000/MSX/
  SVI/Sord/Memotech/Einstein/PV2000/NABU/SMS), 6502 (Creativision/NES) or
  TMS9900 (TI-99/4A) assembler. Assembling the `.asm` is a separate step
  (gasm80, below).
- The submodule directory is `CVBasic/` (uppercase); `copy.cvbasic` extracts
  it into `$(BUILDDIR)/cvbasic` (lowercase) so the generic `%-fs.zip` rule and
  output naming stay consistent. `$(BUILDDIR)/copy.cvbasic`/`cvbasic.wasi` are stamps and
  are cleaned like cc65's.
- Plain C with no subprocesses (`system()`/`popen()` are not used). The build
  is just the upstream Makefile with `CC`/`CPPFLAGS`/`LDFLAGS` overrides; no
  patches and no native build step.
- The compiler opens `cvbasic_*_prologue.asm` / `cvbasic_*_epilogue.asm` at
  runtime and writes a scratch `cvbasic_temporary.asm` in cwd. The build bakes
  in `-DASM_LIBRARY_PATH="/share/cvbasic"`, and `cvbasic-fs.zip` ships those
  files at `share/cvbasic/`, so no explicit library path is needed (the
  optional third argument still overrides it).
- The generated `.asm` embeds the command line and an `asctime()` timestamp;
  `test.cvbasic` filters the `; Command:`/`; Created:` lines before comparing
  the goldens (Z80 Colecovision, `--nes` 6502 and `--ti994a` TMS9900 output).
  It also assembles the Colecovision output with gasm80 and checks the size
  (8192) and `$55AA` cartridge header of the resulting ROM.

## gasm80 (WASI)

- Single-file C assembler by the same author, the second half of the CVBasic
  toolchain (Z80 + 6502, `-d` defines, `-l` listing, `-s` symbol file, `-sms`
  checksum patch). No runtime data and no `INCLUDE` search path: it opens
  includes/incbins relative to cwd, which suits the IDE's preopened root.
- Build is one clang invocation against `gasm80.c`; the big stack size covers
  its recursive expression parser (`-Wl,-z,stack-size=1048576`).
- `$(BUILDDIR)/copy.gasm80`/`gasm80.wasi` are stamps like cvbasic's. `test.gasm80` runs
  the submodule's own Z80, 6502 and INCBIN tests against their committed
  `.bin` goldens.

## macross + slinky (WASI)

- Chip Morningstar's macro cross assembler for the 6502 (Habitat toolchain,
  Museum of Art and Digital Entertainment) + Slinky, its relocating linker.
  Plain K&R C; the upstream Makefile's `-m32` is dropped (wasm32 is the
  32-bit target it assumes — not 64-bit clean) and old diagnostics are
  demoted (`-Wno-error=incompatible-pointer-types` etc.). Slinky builds
  clean; it needs macross's generated `../y.tab.h`, so build macross first.
- `patches/macross-wasi.patch` de-duplicates the tentative definitions that
  `-fcommon` used to merge: wasi-sdk's clang **crashes on any common symbol**
  (even `int x;`), and `-Wl,-z,muldefs` picks the zero-initialized copies of
  `UndefinedValue`/`codeRegions`/`validSymbolValues` over the initialized
  ones in `macrossTables_*.c`, silently breaking assembly. The patch keeps
  one canonical definition each and adds `extern` decls where the removed
  tentative def was the only declaration (db*, tablevel, encodingFunction,
  longLineFlag, nullStatementFlag). It also redirects `fopen("/dev/null")`
  (absent in WASI) to `macross_devnull.tmp` and makes the pass-2 templates
  cwd-relative (`/tmp/z*XXXXXX` doesn't exist in the sandbox).
- `wasi-stubs.c` (generated in the recipe, cf. naken_asm's stubs) supplies
  `mkstemp` (wasi-libc doesn't implement it; pass-2 files in listing mode —
  `O_CREAT|O_EXCL` with a counter suffix, bounded retries so a missing dir
  can't hang) and `end` (main.c's `sbrk` high-water diagnostic).
- `$(BUILDDIR)/copy.macross`/`macross.wasi` are stamps like cvbasic's, but the build dir
  is re-extracted on every `macross.wasi` run so the patch never applies twice.
- The 68000 variant is **not** built: `macross_68000.y` has both `':'` and
  `': '` rules, which modern bison rejects as the same token.
- `test.macross`: `macross -c -o test.out -l test.lst test.m` (a label +
  `lda #0`/`jmp`), cmp object + listing goldens, then `slinky -o linked.out`
  and cmp the linked image.

## Tests

- Layout: `tests/<tool>/` with input sources and golden outputs
  (`*.expected`); generate goldens from an actual run, then commit
- Add a `test.<tool>` target: copy wasm + sources to
  `$(BUILDDIR)/test-<tool>/`, run under `$(WASIRUN)` (default `wasmtime`,
  e.g. `wasmtime --dir=. tool.wasm ...`), `cmp` against the golden
- Register in the `test:` aggregate and `.PHONY`
- wasmtime only exposes the preopened `--dir=.` sandbox: inputs must be
  copied inside it, absolute paths to test files will not resolve. Every
  wasmtime in a pipeline needs its own --dir (2600basic writes headers to cwd)
- c2t: c2t.h (loader code) ships pre-generated in the submodule — do NOT
  run its makeheader: the host cl65 may reject `--list-bytes 100` or fail
  diskload3.s range checks, and repo-cc65 ld65 needs CC65_HOME + `make lib`
- merlin32: setjmp/longjmp needs wasm EH. Compile flags:
  `-mllvm -wasm-enable-sjlj -mllvm -wasm-use-legacy-eh=false` and link
  with `-lsetjmp`. Legacy EH (default sjlj lowering) compiles but wasmtime
  rejects it; forcing `wasm-use-legacy-eh=false` produces standard EH
- batariBasic: 2600basic reads stdin / writes asm to stdout (pipeline:
  preprocess file | 2600basic > out.asm); needs the bB includes/ dir in cwd
- GNUmakefile quoting trap: overriding CFLAGS on the make command line
  strips the escaping from `-DSTR="x"` style defines — sed the Makefile
  rule itself instead (merlin32's -DMACRO_DIR)
- zmac: platform guards like `#if defined(__APPLE__) || defined(__linux__) ||
  defined(EMSCRIPTEN)` don't cover `__wasi__`. Workarounds: `-include
  unistd.h` on CFLAGS — but only on TUs that need it (doc.c defines its own
  static `link()` that collides with unistd.h, so compile doc.o separately
  without the flag). Its `doc` generator must be built/run on the host first
- naken_asm: its custom `./configure` hardcodes `CC=gcc` and ignores args —
  sed `config.mak` afterwards. The build needs `build/{asm,disasm,common,
  table,fileio}` dirs created (configure doesn't). Simulator objects use
  `signal()` (no signals in WASI) and prog objects use serial — remove
  `$(SIM_OBJS)`/`$(PROG_OBJS)` from build/Makefile and add a stubs.c with
  `simulate_init_*` no-ops to satisfy cpu_list.c; the naken_util link is
  stubbed to `echo` like the old emcc flow

## Pitfalls found so far

- Newer clang defaults to C23; old code with `typedef enum boolean
  { false, true }` (yasm's bitvect.h) fails — build with `-std=c99`
- WASI libc hides POSIX functions behind feature macros: add
  `-D_GNU_SOURCE` (not just `_POSIX_C_SOURCE`; strsep needs _BSD/_GNU)
- WASI libc has no `tmpfile()` implementation (deprecated decl only,
  undefined at link). yasm's dbg objfmt used it — patched with sed to
  `fopen("yasm-dbg.out", "w+")` in the build dir
- Old bundled `config.sub`/`config.guess` don't know `wasm32-wasi` —
  copy newer ones from `/opt/homebrew/share/automake-1.18/`
  (Makefile detects this via `NEW_CONFIG_SUB`)
- yasm autogen requires re2c and bison (bison >= 2.4 preferred);
  macOS bison is 2.3 but worked
- `git submodule set-url` only fixes `.git/config`; commit the `.gitmodules`
  change too (acme's URL was a broken relative path, now `sehugg/acme`;
  dasm now tracks upstream `dasm-assembler/dasm` @ v2.20.17-3)
- The old dasm fork had local patches (listing filename, exit status,
  xmacro.h) upstream may not have — verify IDE stderr-parsing when wiring
  into the worker
