<p align="center">
  <img src="IRVana.png" width=60% height=60%>
</p>

*IRvana is a learning and experimentation project centered around the LLVM framework and its intermediate representation (IR) formats, with a focus on code execution, reverse engineering, and JIT experimentation.
While its roots are in offensive research, IRvana can be equally valuable for LLVM enthusiasts and developers looking to explore the inner workings of LLVM IR, its tooling, and its practical applications for IR debugging, transformation, and multi-language analysis. These experiments have been specifically targetted for the Windows platform. This choice is intentional, as Windows remains the primary environment of interest for red teams/offensive security and LLVM based Windows development is relatively less widespread compared to Unix like systems.*

Project features:
1. Simplify Cross-Language LLVM IR Generation
    - Provide streamlined IR generation for four languages: C, C++, Rust, and Nim
    - Integrate OLLVM obfuscation passes to transform and optimize the generated IR
    - Automate IR linking and enable direct interpretation using LLVM tooling
2. Enable In-Memory LLVM IR Execution via JIT and BYOI
    - Leverage potential lolbas style execution with LLVM’s lli interpreter
    - Working POCs using ORCJIT and MCJIT for runtime IR execution
    - Showcase malware development techniques and integration with LLVM interpreters for improved OPSEC

Here's an index of the project documentation:
- [Introduction](#Introduction)
- [Project Structure](#project-structure)
- [Setup Prerequisites](#setup-prerequisites)
- [Streamlining IR Generation with IRvana](IRgen/README.md#llvm-ir--streamlining-generation-with-irvana)
  - [Streamlined Generation with IRvana](IRgen/README.md#streamlined-generation-with-irvana)
  - [LLVM IR generation for C programs](IRgen/c/README.md#llvm-ir-generation-for-c-programs)
  - [LLVM IR generation for C++ programs](IRgen/cxx/README.md#llvm-ir-generation-for-c-programs)
  - [LLVM IR generation for Nim programs](IRgen/nim/README.md#llvm-ir-generation-for-nim-programs)  
  - [LLVM IR generation for Rust programs](IRgen/rust/README.md#llvm-ir-generation-for-rust-programs)
- [OLLVM and IRvana integration](OLLVM/README.md#ollvm-pass)
- [LLVM Interpreters](Interpreters/README.md#llvm-interpreters)
  - [LLVM lli Tool (Part of LLVM toolchain)](Interpreters/lli%20-%20ORC%20JIT/README.md#llvm-lli-tool-part-of-llvm-toolchain)
  - [C++ MCJIT Interpreter](Interpreters/cxx%20-%20MCJIT/README.md#c-mcjit-interpreter)
  - [C++ ORCJIT Interpreter](Interpreters/cxx%20-%20ORC%20JIT/README.md#c-orcjit-interpreter)
  - [Rust-Based LLVM IR Interpreter (via inkwell + MCJIT)](Interpreters/Rust%20-%20MCJIT/README.md#rust-based-llvm-ir-interpreter-via-inkwell--mcjit) 
- [Malware Development and LLVM interpreters](Maldev/README.md#malware-development-and-llvm-interpreters)
  - [IR Payload Encryption](Maldev/IREncryption/README.md#ir-payload-encryption)
  - [IR Payload Staging from Remote Server](Maldev/RemoteLoad/README.md#ir-payload-staging-from-remote-server)
- [Conclusion and Credits](#conclusion-and-credits)

## Introduction

LLVM is a powerful compiler infrastructure that supports multiple frontends like C, C++, Rust, and more. It compiles these high-level languages into a target-independent intermediate representation (IR) before generating final machine code.

<p align="center">
  <img src="https://i.sstatic.net/9xGDe.png" width=60% height=60%>
</p>

LLVM IR is like assembly for the LLVM toolchain low-level, human-readable, and optimized for analysis and transformation. There are two key IR formats:
- `.ll` – Plain text form of LLVM IR
- `.bc` – Binary (bitcode) form of LLVM IR 

In conventional development workflows, LLVM IR is an intermediate step toward creating native binaries. However, LLVM also supports JIT execution via tools like lli or custom runtimes based on ORC/MCJIT. This enables IR to be directly executed in memory making it a lightweight, portable, and dynamic format suitable for use as a stage 1 payload in red teaming or offensive tooling using the BYOI (Bring your own Interpreter) concept.

Converting between .ll and .bc is seamless and straightforward. There are two common approaches to generate LLVM IR for JIT execution:
- Manual IR authoring: Writing .ll files by hand offers readability and control, making it more approachable than raw assembly. However, this method becomes impractical for large codebases due to its complexity and verbosity.
- Automated IR generation: Projects like IRvana can generate LLVM IR from raw, cross-language source code using LLVM tools and apply optimization or deoptimization technique like obfuscation. This makes the resulting IR significantly harder to reverse engineer or analyze.

What makes LLVM IR especially compelling is that IR generated from multiple frontends C, C++, Rust, and Nim can all be interpreted by the same engine. This allows for cross-language polymorphism, dynamic payload behavior, and modular development of payloads and tooling. The interpretability and flexibility of IR make it a powerful format for both learning and offense, where runtime evasion, transformation, and obfuscation are key.

To realize this vision, LLVM version compatibility is critical. Small differences between compiler versions (e.g., Clang, Rustc) can break IR compatibility due to differences in metadata, intrinsics, or ABI details. To reach an "IRvana" state this project standardizes IR generation on a single LLVM version (18.1.5) across all frontends, ensuring the IR produced is linked and stable for execution through lli, ORCJIT, MCJIT, or custom interpreters. By aligning compiler flags, links, and toolchains, IRvana helps build a consistent IR pipeline to generate IR from C, C++, Rust and Nim project sources with multiple source files and includes.

This project also integrates obfuscation at the IR level using OLLVM. Techniques such as control flow flattening, indirect calls, string encryption, and indirect branching are applied to harden the IR and simulate real-world evasive payloads. These techniques are applied post-linking or on per-file basis, depending on the obfuscation mode selected.

Lastly, IRvana includes built-in LLVM tools and multiple POCs in C++ and Rust that demonstrate JIT (Just in time) execution using ORCJIT and MCJIT capable of interpreting generated IR. Maldev related integrations such as in-memory IR loading and encrypted payload decryption have also been documented for real world applications. These examples bridge LLVM IR tooling with malware development practices to explore JIT code execution and BYOI techniques in depth.

# Main Project structure

```
IRVana/
├── IRgen/
│   ├── c/
│   │   └── Makefile.ir.mk --> Makefile for IR generation
|   |   └── src --> User places source here
|   |   └── ir_bin --> IR Generated file named final.ll or final-obf.ll
|   |   └── detect_sdk.bat --> Windows SDK and runtime detection
|   |   └── vs_env.mk --> Windows SDK and runtime generated information
│   ├── cxx/
│   │   └── Makefile.ir.mk
|   |   └── src --> User places source here
|   |   └── ir_bin --> IR Generated file named final.ll or final-obf.ll
|   |   └── detect_sdk.bat --> Windows SDK and runtime detection
|   |   └── vs_env.mk --> Windows SDK and runtime generated information
│   ├── rust/
│   │   └── Makefile.ir.mk
|   |   └── src --> User places source here
|   |   └── ir_bin --> IR Generated file named final.ll or final-obf.ll
|   |   └── detect_sdk.bat --> Windows SDK and runtime detection
|   |   └── vs_env.mk --> Windows SDK and runtime generated information
│   └── nim/
│   |   └── Makefile.ir.mk
|   |   └── src --> User places source here
|   |   └── ir_bin --> IR Generated file named final.ll or final-obf.ll
|   |   └── detect_sdk.bat --> Windows SDK and runtime detection
|   |   └── vs_env.mk --> Windows SDK and runtime generated information
├── LLVM-18.1.5/
│   |   └── bin  --> Executable binaries like clang, clang++, clang-cl, opt, llc, lli, etc.
|   |   └── include --> header files for LLVM and Clang APIs
|   |   └── lib --> Static and dynamic libraries for LLVM/Clang.
|   |   └── libexec --> Helper executables used internally by LLVM/Clang
├── OLLVM/
|   |   └── vs_build --> Build files for OLLVM DLL
|   |   |   └── ollvm-pass.sln --> Visual Studio project to recompile OLLVM DLL
|   |   |   └── obfuscation\Release\LLVMObfuscationx.dll --> Precompiled OLLVM DLL
|   |   └── .....
IRvana.cpp --> Source code for automating IR generation and obfuscation
IRvana.sln --> Primary project for handling IR generation and obfuscation
├── x64/
│   └── Debug/
│       └── IRVana.exe --> Compiled build
├── nim-1.6.6/ 
│   |   └── bin  --> Executable binaries like nim, nimble
│   |   └── compiler --> Nim compiler sources and modules 
│   |   └── lib  --> Standard libraries and runtime used by Nim 
├── Interpreters/
|   |   └── lli - ORC JIT --> LLVM’s own command-line interpreter
|   |   └── cxx - MCJIT --> Custom C++ interpreter built using the older MCJIT engine
|   |   └── cxx - ORC JIT --> Modernized C++ interpreter leveraging ORC JIT APIs
|   |   └── Rust - MCJIT --> Rust-based interpreter using MCJIT
├── Maldev/
|   |   └── IREncryption --> POC for to AES Encrypt LLVM IR before execution
|   |   └── RemoteLoad --> Loads obfuscated IR over the HTTP and executes via JIT in memory
```

# Installation using a script

You can just run the below commands to install the entire project including setup
```powershell
# Run using an elevated powershell
Set-ExecutionPolicy -Force Bypass
iex(iwr -Uri https://raw.githubusercontent.com/m3rcer/IRvana/refs/heads/main/Install.ps1 -UseBasicParsing)
```

# Docker container
```docker
docker build -t irvana-windows .
docker run -it --rm -v <Host machine folder>:C:\IRvana\Shared irvana-windows
```

# Manual Installation

## Setup Prerequisites

These steps are prerequisites required before using the project and interpreters. To look at build instructions for IRvana.sln and interpreters, please reference README.md in their respective directories.

### Clone or download project source

Install git and clone project source on Windows.

```PowerShell
> git clone https://github.com/m3rcer/IRvana.git
> cd IRvana
```

### Install prerequisite toolsets

Install Visual Studio and make sure you have VCRuntime and Windows SDK installed:
- https://aka.ms/vs/17/release/vc_redist.x64.exe
- https://developer.microsoft.com/en-us/windows/downloads/windows-sdk/

Perform this simple check to ensure you have these toolsets installed and detected by `detect_sdk.bat`:

```powershell
IRvana> .\IRgen\c\detect_sdk.bat

IRvana> type vs_env.mk
winsdkdir := C:\\Program Files (x86)\\Windows Kits\\10\\
vctoolsdir := C:\\Program Files\\Microsoft Visual Studio\\2022\\Community\\VC\\Tools\\MSVC\\14.40.33807
sdkver := 10.0.22621.0
```

Install make for windows and make sure you have it added to Environment PATH: https://gnuwin32.sourceforge.net/packages/make.htm. 

```powershell
IRvana> setx PATH "%PATH%;C:\Program Files (x86)\GnuWin32\bin"
```

Use the installer for easy installation.

```powershell
IRvana> make -v
GNU Make 3.81
Copyright (C) 2006  Free Software Foundation, Inc.
This is free software; see the source for copying conditions.
There is NO warranty; not even for MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE.

This program built for i386-pc-mingw32
```

Similarly, download and install cmake for Windows and make sure it is added to Environment PATH: <https://cmake.org/download/>

```powershell
IRvana> cmake --version
cmake version 4.1.0-rc4

CMake suite maintained and supported by Kitware (kitware.com/cmake).

FLARE-VM 01-08-2025 15:54:03.34
```

### Install LLVM 18.1.5

Download precompiled LLVM 18.1.5 from LLVM's official GitHub repo (Optional: compile from source): <https://github.com/llvm/llvm-project/releases/download/llvmorg-18.1.5/clang+llvm-18.1.5-x86_64-pc-windows-msvc.tar.xz>

Extract it and place it in the IRvana project root and rename the directory to `LLVM-18.1.5`

*NOTE: Avoid using tar in windows as this may not work, use alternatives like Winrar for extraction*

Make sure the directory structure is as follows:
```
## IRvana project root
├── LLVM-18.1.5/
│   |   └── bin  --> Executable binaries like clang, clang++, clang-cl, opt, llc, lli, etc.
|   |   └── include --> header files for LLVM and Clang APIs
|   |   └── lib --> Static and dynamic libraries for LLVM/Clang.
|   |   └── libexec --> Helper executables used internally by LLVM/Clang
```

Add the toolchain directory to Environment PATH for quick debugging and execution.

```powershell
IRvana> setx PATH "%PATH%;C:\IRvana\LLVM-18.1.5"
```

### OLLVM for LLVM 18.1.5 (optional)

This step is optional as OLLVM source is already placed in project root and precompiled. 
To optionally recompile the OLLVM DLL, a Visual Studio project has been generated with cmake in the IRvana source. More details can be found [here].

Make sure your OLLVM build follows the following directory structure.
```
## IRvana project root
├── OLLVM/
|   |   └── build --> Build files for OLLVM DLL
|   |   |   └── ollvm-pass.sln --> Visual Studio project to recompile OLLVM DLL
|   |   |   └── obfuscation\Release\LLVMObfuscationx.dll --> Precompiled OLLVM DLL
```

A Visual Studio solution is included for quick reference along with a precompiled version of the plugin. If you wish to recompile the obfuscation DLL from source, follow the steps below:

```powershell
## Generate Build Files
IRvana> cd OLLVM
IRvana\OLLVM> cmake -G "Visual Studio 17 2022" -S .\ollvm-pass -B .\build ^
  -DCMAKE_CXX_STANDARD=17 ^
  -DCMAKE_BUILD_TYPE=Release ^
  -DBUILD_SHARED_LIBS=ON ^
  -DLT_LLVM_INSTALL_DIR=C:\IRvana\LLVM-18.1.5\

## Build the DLL
IRvana\OLLVM> cmake --build .\build --config Release

## Integrate with IRvana
IRvana\OLLVM> del vs_build  
IRvana\OLLVM> move build vs_build
```

>Replace `C:\IRvana\LLVM-18.1.5\` with your actual LLVM installation path.

###  Install Nim 1.6.6

Nim 1.6.6 is required to function with LLVM 18.1.5.

Download Nim 1.6.6, extract it in the IRvana root: <https://nim-lang.org/download/nim-1.6.6_x64.zip>

Place it in the IRvana root as nim-1.6.6 following this directory structure. Optionally add this path to Environment PATH.

```
## IRvana project root
├── nim-1.6.6/  --> Nim binaries and libs for Nim IR generation
│   |   └── bin  --> Executable binaries like `nim`, `nimble`, and `nimgrep`
│   |   └── compiler     --> Nim compiler sources and modules (used internally by `nim`)
│   |   └── lib  --> Standard libraries and runtime used by Nim programs
|   |   └── .....
```

###  Install Rust 1.81.0 nightly

`rustc 1.81.0-nightly` is required to function with LLVM 18.1.5.

Download and install it using rustup.
- Install rustup: <https://static.rust-lang.org/rustup/dist/x86_64-pc-windows-msvc/rustup-init.exe>
- Install nightly toolchain: `rustup toolchain install nightly-2024-06-26`

Make sure you have the toolchain and other supporting components installed.

```
IRVana> cargo +nightly-2024-06-26 rustc -h
info: syncing channel updates for 'nightly-2024-06-26-x86_64-pc-windows-msvc'
info: latest update on 2024-06-26, rust version 1.81.0-nightly (fda509e81 2024-06-25)
info: downloading component 'cargo'
info: downloading component 'clippy'
info: downloading component 'rust-docs'
info: downloading component 'rust-std'
info: downloading component 'rustc'
info: downloading component 'rustfmt'
info: installing component 'cargo'
info: installing component 'clippy'
info: installing component 'rust-docs'
info: installing component 'rust-std'
info: installing component 'rustc'
info: installing component 'rustfmt'
Compile a package, and pass extra options to the compiler
Usage: cargo.exe rustc [OPTIONS] [ARGS]...
```

## Conclusion and Credits

LLVM IR sits in a unique niche, low-level enough to allow control over code execution flow, yet high-level enough to support complex cross-language abstractions. Combined with JIT and obfuscation using LLVM, it becomes an excellent platform for learning, prototyping, adversarial tooling for stage 1 payloads and interpreter driven execution.

All pull requests aimed at improving IR generation stability, adding support for additional front-end languages, advancing interpreter development, enhancing maldev techniques, or contributing new ideas are always appreciated.

Project references and credits:
- <https://github.com/0xlane/ollvm-rust>
- <https://github.com/trustedsec/LLVM-Obfuscation-Experiments>
- <https://llvm.org/>
- <https://github.com/llvm/llvm-project>
- <https://mcyoung.xyz/2023/08/01/llvm-ir/>


