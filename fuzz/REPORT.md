# Static Analysis & Fuzzing Report — TierOne-Software/srec

Date: 2026-11-07 · Commit analyzed: `51ce994` ("Forget Windows.", 2025-07-06) · Clone: `/home/mj/workspaces/tierone/srec`

> **Resolution (2026-11-07):** all findings F1-F9 are fixed in the commits
> landing with this report. `convert_srec_to_bin` and `sreccheck` now run on
> `SrecStreamParser` (strict hex, byte-count/length and checksum validation);
> `write_header(vector<string>)` writes raw bytes; CLI tools exit non-zero on
> error; `convert_stream` rejects `buffer_size == 0`; `parse_line` reports a
> clean validation error for too-small byte counts. Regression tests cover
> every fixed finding (unit + `roundtrip_test.sh` malformed-input cases), and
> the pre-fix fuzz crash artifact now runs clean through libFuzzer.

## Method

| Step | Tooling | Result |
|---|---|---|
| Unit + integration tests | CMake/Ninja, Catch2, ctest | 20/20 pass |
| Sanitizer test run | clang 22, ASan + UBSan | 20/20 pass, no reports |
| Static analysis | clang-tidy (clang-analyzer, bugprone, cert, performance, portability) | 1 real bug class, style noise |
| Fuzzing (parser API) | libFuzzer + ASan + UBSan, `fuzz_parse` | 3,803,817 execs, 0 crashes |
| Fuzzing (srec→bin) | libFuzzer + ASan + UBSan, `fuzz_srec_to_bin` | crash found in seconds (F2); after masking F2, 6,311,699 execs, no further crashes |
| Fuzzing (roundtrip diff) | libFuzzer + ASan + UBSan, `fuzz_roundtrip` (bin→srec→bin compare) | 1,563,417 execs, 0 mismatches |

Total: ~11.7M fuzz executions. **No memory-safety errors found** — the parser's
bounds checks (`parse_hex_byte`, length validation) hold up. The findings below
are crash-on-exceptions, integrity, and robustness issues.

Harnesses live in `fuzz/` (`fuzz_parse.cpp`, `fuzz_srec_to_bin.cpp`,
`fuzz_roundtrip.cpp`, plus `demo_hdr.cpp`, `demo_misc.cpp` reproductions).
Build: `clang++ -std=c++17 -g -O1 -fsanitize=fuzzer,address,undefined -I. fuzz/<h>.cpp srec/srec.cpp -o fuzz/<h>`.

---

## F1 — CRASH: `sreccheck` aborts on malformed input (uncaught `std::invalid_argument`)

**Severity: High** (untrusted file → `std::terminate` / SIGABRT, exit 134)

`std::stoul` throws `std::invalid_argument` on non-hex input. Neither call site
handles it:

- `sreccheck.cpp:71` — S0 CRC parse; the `catch` covers only `std::out_of_range`.
- `sreccheck.cpp:95` — data-area hex parse; no `try` at all.

Repro (both abort with `terminate called after throwing an instance of
'std::invalid_argument'`):

```
printf 'S0060000GG\n' > bad.srec && sreccheck bad.srec      # SIGABRT (S0 path)
printf 'S1070000ZZ00\n' > bad.srec && sreccheck bad.srec    # SIGABRT (data path)
```

clang-tidy independently flags this: `bugprone-exception-escape` on
`sreccheck.cpp:26` (`main` may throw).

**Fix:** wrap the parse loop body in `try/catch (const std::exception &)` and
report the line as invalid (non-zero exit), or use the library's
`SrecStreamParser` which already validates hex strictly.

## F2 — CRASH: undocumented `std::out_of_range` escapes `convert_srec_to_bin`

**Severity: High** (contract violation; crashes callers that trust the docs)

Any `S1`/`S2`/`S3` line shorter than `4 + 2*addr_bytes + 2` chars reaches
`line.substr(...)` past the end (`srec/srec.cpp:133-140`) and throws
`std::out_of_range`. The API documents only `std::ios_base::failure`.
Fuzz artifact `fuzz/artifacts_bin/crash-4d78f60b...` (first line `S1B`):

```
terminate called after throwing an instance of 'std::out_of_range'
  what():  basic_string::substr: __pos (which is 8) > this->size() (which is 3)
```

`srec2bin`'s `main` happens to catch `std::exception`, so the CLI survives but
still exits 0 (see F6). Any other caller catching only the documented type
crashes.

**Fix:** validate `line.size()` before `substr` (or use `SrecStreamParser`) and
throw `SrecParseException` / `ios_base::failure` as documented.

## F3 — Data corruption: lax hex parsing silently accepts garbage

**Severity: Medium** (silent wrong output from corrupt input)

`std::stoi`/`std::stoul` stop at the first invalid character instead of
rejecting it. Demonstrated (`fuzz/demo_misc.cpp`):

```
input data field "1Z" converted to byte 0x1 with NO error (expected: parse error)
```

A record whose data contains `1Z`, `+1`, ` 1`, etc. converts "successfully"
with silently wrong bytes — in `convert_srec_to_bin` (`srec/srec.cpp:147`) and
`sreccheck` (`sreccheck.cpp:71,95`, which computes CRCs over garbage
interpretations). Only `SrecStreamParser::hex_char_to_byte` is strict.

**Fix:** route all hex parsing through a strict nibble parser that rejects any
non-`[0-9A-Fa-f]` character.

## F4 — `write_header(vector<string>)` double-hex-encodes the header

**Severity: Medium** (wrong S0 output; masked by a vacuous test)

`SrecFile::write_header(const std::vector<std::string>&)` runs the string
through `ASCIIToHexString`, then feeds the resulting **hex text** to
`Srec0(const std::string&)`, which stores each character as a byte
(`srec/srec.cpp:291-303`). Repro (`fuzz/demo_hdr.cpp`):

```
write_header({"TEST"})          -> S00B0000353434353533353451   # bytes = ASCII "54455354"  (wrong)
write_header({'T','E','S','T'}) -> S007000054455354B8           # bytes = 54 45 53 54 = "TEST" (right)
```

The unit test `"write_header from strings"` (`test/test.cpp:531-546`) computes
`expected` and then **never asserts it** ("Note: This doesn't match what we
expect...") — the bug ships behind a green test.

**Fix:** pass the raw bytes to `Srec0` (drop the `ASCIIToHexString` call or use
the `vector<uint8_t>` overload); add the missing `REQUIRE(line == expected)`.

## F5 — `convert_srec_to_bin` ignores record structure entirely

**Severity: Medium** (integrity)

The byte-count field is never checked against the line length, and record
checksums are never verified (`srec/srec.cpp:107-159`). It strips a fixed-size
address window and the last 2 chars, then decodes whatever is between. Truncated
or tampered records silently yield wrong binary output with exit 0. (By
contrast, `SrecStreamParser::parse_line` validates both.)

**Fix:** validate count-vs-length and checksum, or reimplement on top of
`SrecStreamParser`.

## F6 — `srec2bin` exits 0 on failure

**Severity: Low**

`main` catches the conversion error, prints it, and falls through to
`return 0` (`srec2bin.cpp:61-67`) — same for a failed open. Scripts cannot
detect failure.

**Fix:** `return 1;` in the catch block.

## F7 — `convert_stream(buffer_size=0)` spins writing empty records

**Severity: Low** (API misuse → garbage file + stall instead of an error)

`chunk_size = std::min(buffer_size, max_record_size)` becomes 0; the read loop
then never consumes input and writes one empty record per iteration until
`MAX_RECORD_COUNT` (1,000,000) throws. Measured (`fuzz/demo_misc.cpp`): 5-byte
input produced a 1,000,000-line (~19 MB) file in ~1 s before
"Maximum record count exceeded".

**Fix:** reject `buffer_size == 0` with `std::invalid_argument` up front.

## F8 — Ignored `std::rename` result in `write_checksum`

**Severity: Low** (clang-tidy `cert-err33-c`, `srec/srec.cpp:104`)

If the rename fails the checksum-prepended file is silently not installed.

## F9 — `parse_line`: unsigned underflow gives a misleading error

**Severity: Low** (error quality, not memory safety)

For `byte_count < addr_bytes + 1` (e.g. `S50200XX`), `data_bytes` underflows
(`srec/srec.cpp:441`), `reserve()` throws `length_error`, and the user sees
"Parse error: ..." instead of a clean "byte count too small for this record
type". Bounded and wrapped as `SrecParseException`, so not exploitable.

**Fix:** check `byte_count >= address_bytes + 1` before subtracting.

## Static-analysis notes (style / hardening, no functional impact)

- Signedness conversions when assembling addresses (`srec/srec.cpp:400-434`):
  `int` → `uint32_t`; add explicit casts.
- 64→32 narrowing (`srec/srec.cpp:213`, `sreccheck.cpp:97`).
- `parse_line`'s type switch has no `default:` (safe today because
  `char_to_type` throws, but fragile) and two identical branches (S0/S1/S9).
- `performance-unnecessary-value-param` on the `std::function` callback
  parameters (`srec/srec.cpp:496,543,557`) — take by const-ref/move.
- Cosmetic: `std::endl` → `'\n'` where flushing is not wanted, enum base
  types, `#pragma once` vs include guards (project-wide, probably intentional).

## Bottom line

The core `SrecStreamParser` and the writer path are **memory-safe and
roundtrip-consistent** under 11.7M fuzz executions and ASan/UBSan. The risk is
concentrated in the **hand-rolled parsing in `convert_srec_to_bin` and
`sreccheck`**: one confirmed abort-on-input crash (F1), one undocumented
exception (F2), and silent data corruption on malformed hex (F3). All three
would be eliminated by routing those utilities through `SrecStreamParser`.
