# C2 Integrations for IR Based JIT Execution

`LLVM-C.dll` exposes ORC JIT as a C API. The BOF loads it at runtime, resolves symbols via GetProcAddress, and JIT-compiles .ll/.bc to a native function pointer in process for execution. 


## Execution flow

BOF (Beacon Object File) integrations that enable in process LLVM IR execution directly inside a C2 beacon.

1. **Stage**: upload `LLVM-C.dll` to the victim once per host. This is the only file that touches disk.
2. **Execute**: send `.ll` text or `.bc` bitcode as BOF arguments from the operator machine. The BOF loads LLVM-C.dll, JIT-compiles the IR in-process via ORC JIT, and calls the entry function. Output is captured and returned through the beacon's callback channel.

```
operator → [ ir_exec payload.ll ] → BOF args (IR bytes in-memory)
                                          ↓
                              beacon process (victim)
                              LoadLibrary(LLVM-C.dll)
                              LLJIT → JIT-compile IR
                              call fnName(argc, argv)
                              output → BeaconPrintf
```

IR args are passed as `argv[1..n]` with `"illi"` auto-set as `argv[0]`, matching standard C `main(int argc, char *argv[])` convention.

Extra DLLs needed by the IR (`user32.dll`, `ole32.dll`, etc.) can be loaded before JIT — equivalent to `lli.exe --load=<dll>`.

`LLVM-C.dll` can also be loaded entirely in memory.


### BOF: illi for AdaptixC2

| File | Description |
|---|---|
| `illi_orc_bof.c` | BOF source  ORC JIT / LLJIT, stdio capture, argv building |
| `illi_orc_bof.x64.o` | Precompiled x64 BOF |
| `build.bat` | Compile using IRvana's bundled clang |
| `illi.axs` | Adaptix plugin registers `ir_stage` and `ir_exec` commands |
| `LLVM-C.dll` | LLVM 18 JIT engine |

**Commands:**

```
ir_stage /opt/illi/LLVM-C.dll
    Upload LLVM-C.dll to victim (once per host)

ir_exec <file.ll> [fn_name] [load_libs] [ir_args] [dll_path] [--async]
    JIT-compile and run IR inside the beacon
```

**Build:**
```bat
.\build.bat
```

Uses `clang-cl.exe` from IRvana's bundled `LLVM-18.1.5\bin\` and no separate toolchain required.

