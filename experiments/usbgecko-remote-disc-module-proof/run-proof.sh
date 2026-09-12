#!/usr/bin/env bash
set -euo pipefail

HERE="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"

DEVICE="${DEVICE:-/dev/serial/by-id/usb-USBGECKO_USB_Gecko_Development_Adapter_GECKUSB0-if00-port0}"

IMAGE="${IMAGE:-$HOME/Programming/Games/doomcube/doomcube-v1.1.1-dev-aa263d5-dirty.iso}"

EXPECTED_IMAGE_SHA="52c4b2e8626af77bdb2d797028493a44ffa577cf77604ce98429c943c4a36345"

LOG="$HOME/Downloads/ch-remote-disc-module-proof-$(date +%Y%m%d-%H%M%S).log"

echo "============================================================"
echo "CARRYHANDLE — REMOTE DISC MODULE PROOF #4"
echo "============================================================"
echo
echo "Device : $DEVICE"
echo "Image  : $IMAGE"
echo "Log    : $LOG"
echo

actual_sha="$(sha256sum "$IMAGE" | awk '{print $1}')"

if [ "$actual_sha" != "$EXPECTED_IMAGE_SHA" ]; then
    echo "ERROR: unexpected test-image SHA256:"
    echo "  $actual_sha"
    exit 1
fi

echo "============================================================"
echo "BUILD REAL CARRYHANDLE CONSUMER"
echo "============================================================"
echo

cd "$HERE"

make

NM="/opt/devkitpro/devkitPPC/bin/powerpc-eabi-nm"

echo
echo "============================================================"
echo "MODULE LINK SANITY"
echo "============================================================"
echo

"$NM" "$HERE/ch_remote_disc_module_proof.elf" \
    | grep -E \
        'CH_RemoteDisc(Open|Read|Close|IsOpen)$'

echo
echo "CarryHandle remote-disc symbols linked: PASS"

echo
echo "============================================================"
echo "SEND DOL"
echo "============================================================"
echo
echo "Swiss must have:"
echo "  Enable USB Gecko   = Yes"
echo "  Wait for USB Gecko = No"
echo

make run

echo
echo "============================================================"
echo "WIILOAD COMPLETE — START UNCHANGED PROOF-3 SERVER"
echo "============================================================"
echo

sleep 0.10

set +e

python3 \
    "$HERE/host/ch_remote_read_server_v3.py" \
    "$DEVICE" \
    "$IMAGE" \
    2>&1 | tee "$LOG"

STATUS=${PIPESTATUS[0]}

set -e

echo

if command -v wl-copy >/dev/null 2>&1; then
    wl-copy < "$LOG"
    echo "Copied complete report to clipboard."
elif command -v xclip >/dev/null 2>&1; then
    xclip -selection clipboard < "$LOG"
    echo "Copied complete report to clipboard."
fi

echo
echo "Report:"
echo "  $LOG"
echo

echo "============================================================"

if [ "$STATUS" -eq 0 ]; then
    echo "CH-REMOTE-DISC-MODULE-1: HARDWARE PASS"
else
    echo "CH-REMOTE-DISC-MODULE-1: NOT YET PASSED"
fi

echo "============================================================"

exit "$STATUS"
