#!/usr/bin/env bash
# Signs a built firmware.bin and pushes it to the board: ota-push.sh <board-host> [signing-key] [firmware.bin]
set -euo pipefail
HOST="${1:?usage: ota-push.sh <board-host> [signing-key] [firmware.bin]}"
KEY="${2:-${OTA_SIGNING_KEY_FILE:-$HOME/.config/matrix-dashboard/ota_signing_key.pem}}"
BIN="${3:-.pio/build/adafruit_matrixportal_esp32s3/firmware.bin}"
SIG="$(mktemp)"; trap 'rm -f "$SIG"' EXIT
openssl dgst -sha256 -sign "$KEY" -out "$SIG" "$BIN"
curl -fsS --max-time 120 -H 'Expect:' -H "X-Signature: $(base64 < "$SIG" | tr -d '\n')" \
  --data-binary @"$BIN" "http://$HOST/ota"
echo
