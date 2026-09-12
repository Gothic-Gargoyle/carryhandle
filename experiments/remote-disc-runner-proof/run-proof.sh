#!/usr/bin/env bash
set -euo pipefail

REPO="$HOME/Programming/Games/carryhandle-hardware-runner"

PROOF="$REPO/experiments/usbgecko-remote-dvd-proof"

IMAGE="${IMAGE:-$HOME/Programming/Games/doomcube/doomcube-v1.1.1-dev-aa263d5-dirty.iso}"

EXPECTED_SHA="52c4b2e8626af77bdb2d797028493a44ffa577cf77604ce98429c943c4a36345"

LOG="$HOME/Downloads/ch-remote-disc-runner-$(date +%Y%m%d-%H%M%S).log"

echo "============================================================"
echo "CARRYHANDLE — run-remote HARDWARE PROOF"
echo "============================================================"
echo
echo "Consumer : $PROOF"
echo "Image    : $IMAGE"
echo "Log      : $LOG"
echo

actual_sha="$(sha256sum "$IMAGE" | awk '{print $1}')"

if [ "$actual_sha" != "$EXPECTED_SHA" ]; then
    echo "ERROR: unexpected proof image SHA256:"
    echo "  $actual_sha"
    exit 1
fi

echo "This must exercise:"
echo
echo "  make run-remote"
echo "      -> existing make run / wiiload"
echo "      -> production tools/remote-disc server"
echo "      -> existing hardware-proven CHPASS6! application"
echo

set +e

make \
    --no-print-directory \
    -C "$PROOF" \
    run-remote \
    REMOTE_IMAGE="$IMAGE" \
    REMOTE_SERVER_ARGS="--exit-marker CHPASS6!" \
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

echo "============================================================"

if [ "$STATUS" -eq 0 ]; then
    echo "CH-REMOTE-DISC-RUNNER-1: PASS"
else
    echo "CH-REMOTE-DISC-RUNNER-1: NOT YET PASSED"
fi

echo "============================================================"

exit "$STATUS"
