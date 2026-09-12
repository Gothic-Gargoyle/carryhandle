#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

IMAGE="${IMAGE:-$HOME/Programming/Games/doomcube/doomcube-v1.1.1-dev-aa263d5-dirty.iso}"

EXPECTED_SHA="52c4b2e8626af77bdb2d797028493a44ffa577cf77604ce98429c943c4a36345"

LOG="$HOME/Downloads/ch-remote-runtime-stress-$(date +%Y%m%d-%H%M%S).log"

echo "============================================================"
echo "CARRYHANDLE — REMOTE dvd:/ RUNTIME STRESS PROOF #7"
echo "============================================================"
echo
echo "Image : $IMAGE"
echo "Log   : $LOG"
echo

ACTUAL_SHA="$(sha256sum "$IMAGE" | awk '{print $1}')"

if [ "$ACTUAL_SHA" != "$EXPECTED_SHA" ]; then
    echo "ERROR: unexpected image SHA256:"
    echo "  $ACTUAL_SHA"
    exit 1
fi

echo "============================================================"
echo "BUILD"
echo "============================================================"
echo

make \
    --no-print-directory \
    -C "$HERE"

echo
echo "============================================================"
echo "LINK SANITY"
echo "============================================================"
echo

NM="/opt/devkitpro/devkitPPC/bin/powerpc-eabi-nm"

"$NM" "$HERE/ch_remote_runtime_stress_proof.elf" \
    | grep -E \
        'CH_DVDMountRemote$|CH_DVDUnmount$|CH_RemoteDisc(Open|Read|Close)$'

echo
echo "Remote runtime stack linked: PASS"

echo
echo "============================================================"
echo "RUN THROUGH PRODUCTION run-remote"
echo "============================================================"
echo

set +e

make \
    --no-print-directory \
    -C "$HERE" \
    run-remote \
    REMOTE_IMAGE="$IMAGE" \
    REMOTE_SERVER_ARGS="--exit-marker CHDONE7! --show-sideband" \
    2>&1 | tee "$LOG"

MAKE_STATUS=${PIPESTATUS[0]}

set -e

echo

RESULT=0

if [ "$MAKE_STATUS" -ne 0 ]; then
    echo "ERROR: make run-remote returned $MAKE_STATUS"
    RESULT=1
fi

SIDEBAND_RESULT="$(
    python3 - "$LOG" <<'PYRESULT'
from pathlib import Path
import sys

log = Path(sys.argv[1]).read_text(
    errors="replace"
)

prefix = "[GC sideband] "

sideband = "".join(
    line.split(prefix, 1)[1]
    for line in log.splitlines()
    if prefix in line
)

if "CHFAIL7!" in sideband:
    print("FAIL")
elif "CHPASS7!" in sideband:
    print("PASS")
else:
    print("NONE")
PYRESULT
)"

case "$SIDEBAND_RESULT" in
    PASS)
        echo "Reconstructed GameCube sideband: CHPASS7!"
        ;;
    FAIL)
        echo "ERROR: reconstructed GameCube sideband contains CHFAIL7!"
        RESULT=1
        ;;
    *)
        echo "ERROR: neither CHPASS7! nor CHFAIL7! found in reconstructed sideband."
        RESULT=1
        ;;
esac

if ! grep -Fq "Marker : 'CHDONE7!'" "$LOG"; then
    echo "ERROR: production server did not receive CHDONE7!."
    RESULT=1
fi

echo
echo "============================================================"
echo "FINAL RESULT"
echo "============================================================"
echo

if [ "$RESULT" -eq 0 ]; then
    echo "CH-REMOTE-DVD-RUNTIME-STRESS-1: PASS"
    echo
    echo "Real DoomCube ISO       : PASS"
    echo "Remote dvd:/ mount      : PASS"
    echo "Whole-file streaming    : PASS"
    echo "Repeated reopen         : PASS"
    echo "Complete-file CRC32     : PASS"
    echo "Random seek/read        : PASS"
    echo "SEEK_END                : PASS"
    echo "Multiple FST files      : PASS"
    echo "Production run-remote   : PASS"
else
    echo "CH-REMOTE-DVD-RUNTIME-STRESS-1: FAIL"
fi

echo
echo "Report:"
echo "  $LOG"

if command -v wl-copy >/dev/null 2>&1; then
    wl-copy < "$LOG"
    echo
    echo "Copied report to clipboard."
elif command -v xclip >/dev/null 2>&1; then
    xclip -selection clipboard < "$LOG"
    echo
    echo "Copied report to clipboard."
fi

exit "$RESULT"
