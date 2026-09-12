#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

DEVICE="${DEVICE:-/dev/serial/by-id/usb-USBGECKO_USB_Gecko_Development_Adapter_GECKUSB0-if00-port0}"

IMAGE="${IMAGE:-$HOME/Programming/Games/doomcube/doomcube-v1.1.1-dev-aa263d5-dirty.iso}"

EXPECTED_SHA="52c4b2e8626af77bdb2d797028493a44ffa577cf77604ce98429c943c4a36345"

LOG="$HOME/Downloads/ch-remote-fst-proof-$(date +%Y%m%d-%H%M%S).log"

echo "============================================================"
echo "CARRYHANDLE — REMOTE FST BOOTSTRAP PROOF #5"
echo "============================================================"
echo
echo "Device : $DEVICE"
echo "Image  : $IMAGE"
echo "Log    : $LOG"
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

cd "$HERE"

make

echo
echo "============================================================"
echo "LINK SANITY"
echo "============================================================"
echo

NM="/opt/devkitpro/devkitPPC/bin/powerpc-eabi-nm"

"$NM" "$HERE/ch_remote_fst_proof.elf" \
    | grep -E \
        'CH_RemoteDisc(Open|Read|Close|IsOpen)$'

echo
echo "Remote-disc module symbols: PASS"

echo
echo "============================================================"
echo "SEND DOL"
echo "============================================================"
echo
echo "Swiss:"
echo "  Enable USB Gecko   = Yes"
echo "  Wait for USB Gecko = No"
echo

make run

echo
echo "============================================================"
echo "WIILOAD COMPLETE — START GENERIC REMOTE DISC SERVER"
echo "============================================================"
echo

sleep 0.10

set +e

python3 \
    "$HERE/host/ch_remote_disc_server.py" \
    "$DEVICE" \
    "$IMAGE" \
    2>&1 | tee "$LOG"

STATUS=${PIPESTATUS[0]}

set -e

echo

if command -v wl-copy >/dev/null 2>&1; then
    wl-copy < "$LOG"
    echo "Copied report to clipboard."
elif command -v xclip >/dev/null 2>&1; then
    xclip -selection clipboard < "$LOG"
    echo "Copied report to clipboard."
fi

echo
echo "Report:"
echo "  $LOG"
echo

exit "$STATUS"
