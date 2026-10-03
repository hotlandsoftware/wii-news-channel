# Decompilation helper scripts

All scripts can be run from anywhere; they work relative to the repository root.
They read the generated `build/HAGE/asm` disassembly and `config/HAGE/symbols.txt`, so run `ninja` first.

| Script | Usage | What it does |
| --- | --- | --- |
| `fasm.py` | `fasm.py <name or address> [count]` | Print clean disassembly for a function (and the next `count-1`). |
| `refs.py` | `refs.py <start> <end>` | List the data and functions a `.text` range references, and whether code outside the range uses the same data. Helps pick split boundaries. |
| `dump.py` | `dump.py <start> <end>` | Hex/int/float dump of any address range in the original DOL. |
| `od.py` | `od.py <unit> [symbol] [-a]` | Side-by-side objdiff (left = original, right = ours). Shows differing lines only unless `-a`. Unit names are like `news/TextButton`. |
| `rep.py` | `rep.py <unit substring>` | Rebuild `report.json` and print per-function match percentages. |
| `ren.py` | `ren.py old=new[:local\|:weak] ...` | Rename symbols in `symbols.txt` (e.g. `fn_80012345=Foo__3BarFv`). |
| `variants.py` | `variants.py <src> <unit> <symbol> <variants.py>` | Try source variants: the variants file defines `OLD` (text in `src`) and `NEWS` (list of replacements). Prints each variant's match percentage and restores the file. |
| `srcsearch.py` | `srcsearch.py <src> <function> [iterations] [--restarts N] [--sym MANGLED]` | Random local search over equivalent source edits of one function (declaration order, setup-statement order with dependencies kept, commutative operand swaps, adjacent independent statement swaps, `x = x op y` vs `x op= y`). Compiles the unit directly (no ninja run, so different files can be searched in parallel), scores with objdiff and writes the best version back only if it improved. Good for register swaps; read the result, as it can leave odd but equivalent code. For C++ give the plain name as `<function>` and the mangled symbol with `--sym`. |
| `refcmp.py` | `refcmp.py <ogws\|smg\|tp> <src> <start> <end> [--mw VER] -- <cflags>` | Compile a file from a reference decomp (in `../decomp-refs` or `$DECOMP_REFS`) and report, per function, how much matches a same-size function in `[start, end)` of our DOL (relocations masked). Use it to pick the reference and flags before porting a platform file (see `docs/platform_layer_map.md`). |
| `align.py` | `align.py <unit> <start> <end> [--apply]` | Align a compiled unit's `.text` functions with the DOL functions in `[start, end)` by size (LCS) and print the renames; `--apply` renames the remaining `fn_` symbols via `ren.py`. Check weak functions by hand: same-size weak copies can pair up wrongly. |
