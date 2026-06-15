# call_stream

`call_stream` is a C++ module exporting `mo_yanxi.call_stream`. It stores a sequence of heterogeneous callables in one contiguous byte buffer and dispatches them in insertion order. The intended use case is a hot command queue that would otherwise be represented as `std::vector<std::move_only_function<...>>`.

It supports:

- `void()`, `void(Args...)`, and `Ret(Args...)` streams.
- Zero-payload dispatch for empty call objects or compatible static `operator()`.
- Inline storage for trivially copyable callables.
- Inline or heap-backed lifetime management for non-trivial callables.
- `reserve`, `clear`, `merge`, `reset_ip`, `continue_execute`,
  `reset_and_execute`, `operator()`, IP state queries, and early stop for
  result callbacks.
- A compile-time exception policy: resumable exceptions by default, or an opt-in
  `noexcept` stream that rejects potentially throwing callables.

## Example

```cpp
import mo_yanxi.call_stream;
import std;

mo_yanxi::call_stream<void(std::vector<int>&)> stream;

stream.emplace_back([](std::vector<int>& out) { out.push_back(1); });
stream.emplace_back([](std::vector<int>& out) { out.push_back(2); });

std::vector<int> values;
stream(values); // operator() resets ip, then executes from the beginning.

stream.reset_and_execute(values);

if(stream.is_finished()) {
    stream.reset_ip();
}
```

For return-value streams, the last `reset_and_execute`, `continue_execute`, or
`operator()` argument is the result callback. A callback returning `true` stops
dispatch; a callback returning `void` consumes all results.

```cpp
mo_yanxi::call_stream<std::uint64_t(std::uint64_t)> stream;

stream.emplace_back([](std::uint64_t x) { return x + 1; });
stream.emplace_back([](std::uint64_t x) { return x + 2; });

std::uint64_t sum = 0;
stream(10, [&](std::uint64_t result) {
    sum += result;
});
```

`continue_execute` starts from the current instruction pointer. Use it to resume
after a partial run, and use `reset_and_execute` or `operator()` when every call
should run from the beginning:

```cpp
if(stream.has_pending_instructions()) {
    stream.continue_execute(10, [&](std::uint64_t result) {
        sum += result;
    });
}
```

## Exception Policy

`call_stream<FnSign>` infers its exception policy from `FnSign`: ordinary
function signatures use `call_stream_exception_policy::resumable`, while
`noexcept` function signatures use `call_stream_exception_policy::nothrow`.
If a stored callable throws during `continue_execute` or `reset_and_execute`,
the stream keeps `current_ip()` at that callable. A later `continue_execute`
resumes from the same instruction, while `reset_and_execute` and `operator()`
restart from the beginning. `is_at_start()`, `is_finished()`,
`is_partially_executed()`, and `has_pending_instructions()` expose the current
IP state.

Add `noexcept` to the function signature to require nothrow execution:

```cpp
mo_yanxi::call_stream<void() noexcept> stream;
```

`noexcept_call_stream<FnSign>` and the explicit third `call_stream` template
argument remain available when an existing call site wants to spell the policy
directly.

In `nothrow` mode, `emplace_back`, `push_back`, and `operator<<` only accept
callables that are `std::is_nothrow_invocable_r_v` for the stream signature.
Return-value callbacks passed to `continue_execute`, `reset_and_execute`, or
`operator()` must also be nothrow. This keeps the
hot dispatch path free of exception recovery state. `nothrow` streams use
noexcept invoker function pointers and compile-time dispatch branches. For
`void`-returning streams, clang builds enable the musttail trampoline by
default when `[[clang::musttail]]` is available, including resumable streams
whose callables may throw. Resumable tail dispatch records the current
instruction in shared dispatch state before each payload call, so exceptions
still resume at the failing instruction. Define
`MO_YANXI_CALL_STREAM_USE_NOEXCEPT_TAIL_DISPATCH=0` to force loop dispatch for
that path.

## Build And Test

The project uses xmake and depends on `gtest` and `benchmark`.

```powershell
xmake f -c -p windows -a x64 -m release --host_project=y --toolchain=msvc -o build\msvc -y
xmake -r -y call_stream.test
.\build\msvc\windows\x64\release\call_stream.test.exe

xmake f -c -p windows -a x64 -m release --host_project=y --toolchain=clang-cl -o build\clang-cl -y
xmake -r -y call_stream.test
.\build\clang-cl\windows\x64\release\call_stream.test.exe
```

The benchmark helper runs the same release/fastest configuration for clang-cl, clang, and MSVC, cleans C++ module build artifacts between targets, runs correctness tests, runs Google Benchmark, and writes a parsed Markdown summary plus a PNG bar chart:

```powershell
python profiling\run_benchmarks.py --min-time 0.12 run
```

To regenerate only the Markdown summary and chart from existing JSON files:

```powershell
python profiling\run_benchmarks.py --min-time 0.12 --repetitions 3 summarize
```

## Benchmark

The benchmark compares `call_stream` against `std::vector<std::move_only_function<...>>` across 4 signatures, 5 workloads, and 64/1024 call counts. Speedup is vector CPU time divided by `call_stream` CPU time.


![call_stream benchmark speedup by toolchain and signature](benchmark_results/speedup_by_signature.png)

Final run, 2026-05-30:

| Toolchain | Faster cases | Geomean | `void()` | `void(bench_context&)` | `void(bench_context&,uint64_t)` | `uint64_t(uint64_t)` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| clang-cl | 25/40 | 1.14x | 1.00x | 1.31x | 1.17x | 1.11x |
| clang | 24/40 | 1.11x | 1.02x | 1.24x | 1.14x | 1.04x |
| MSVC | 17/40 | 1.01x | 1.02x | 1.01x | 1.08x | 0.92x |

Environment:

- Date: 2026-05-30
- CPU: 13th Gen Intel® Core™ i9-13900HX, 24 cores / 32 threads
- OS: Windows 11
- Google Benchmark: v1.9.5 release
- Build: xmake release, `fastest`, `/DNDEBUG`, `/MD`, `/std:c++latest`, AVX/AVX2 enabled

### Exception Policy Benchmark

The benchmark binary also registers `call_stream_allow_exception/...` cases to compare the default resumable exception policy against the matching `noexcept` stream. Both sides use the same nothrow payload callables; the ratio is allow-exception CPU time divided by `noexcept` CPU time, so values above `1.00x` mean the `noexcept` policy is faster.

Run, 2026-06-15:

```powershell
python profiling\run_benchmarks.py --toolchain clang-cl --toolchain clang --toolchain msvc --min-time 0.12 --repetitions 3 --filter "call_stream(_allow_exception)?/.*" --no-plot --no-clean run
```

| Toolchain | `noexcept` faster cases | Geomean ratio | `void()` | `void(bench_context&)` | `void(bench_context&,uint64_t)` | `uint64_t(uint64_t)` |
| --- | ---: | ---: | ---: | ---: | ---: | ---: |
| clang-cl | 33/40 | 1.32x | 1.19x | 1.60x | 1.53x | 1.05x |
| clang | 32/40 | 1.33x | 1.19x | 1.65x | 1.52x | 1.04x |
| MSVC | 24/40 | 1.03x | 1.02x | 1.03x | 1.05x | 1.00x |

Environment:

- Date: 2026-06-15
- CPU: Intel64 Family 6 Model 183 Stepping 1, GenuineIntel, 32 logical CPUs
- OS: Windows 11 10.0.26200
- Google Benchmark: v1.9.5 release
- Build: xmake release, `fastest`, `/DNDEBUG`, `/MD`, `/std:c++latest`, AVX/AVX2 enabled

## Profiling

VTune profiling is scripted separately from Google Benchmark so the sampled target contains only the selected workload loop:

```powershell
xmake f -c -p windows -a x64 -m release --host_project=y --toolchain=clang-cl -o build\clang-cl -y
xmake -r -y call_stream.profile
python profiling\run_vtune_profiles.py run --target build\clang-cl\windows\x64\release\call_stream.profile.exe --preset focused --seconds 3 --calls 1024 --batch 8192 --force
```

Current hotspot reports are under [profiling/results](profiling/results), with the parsed summary at [profiling/results/analysis.md](profiling/results/analysis.md).

## Performance Notes

The strongest wins are short command streams with reference context arguments, especially under clang-cl where tail dispatch is active. Heavy workloads are often limited by the payload computation itself, so dispatch differences shrink.

The latest optimization pass removed per-result state writes for trivially destructible return values and bypassed `std::invoke` for ordinary directly callable objects. That moved the clang-cl `uint64_t(uint64_t)` group from a weak path to a modest win overall, while MSVC remains close to parity.

The exception-policy benchmark shows that `noexcept` mainly helps void-returning context streams on clang-cl/clang, where the geomean ratio is about `1.5x`-`1.6x` by signature. Scalar return streams are close to parity, and MSVC shows only a small overall policy difference.
