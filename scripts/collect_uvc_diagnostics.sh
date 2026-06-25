#!/usr/bin/env bash
set -u

DEVICE="${1:-/dev/video0}"
STAMP="$(date +%Y%m%d-%H%M%S)"
OUT_DIR="${2:-uvc-diagnostics-${STAMP}}"

if [[ ! -e "$DEVICE" ]]; then
  printf 'Error: video device does not exist: %s\n' "$DEVICE" >&2
  exit 2
fi

if ! command -v v4l2-ctl >/dev/null 2>&1; then
  printf 'Error: v4l2-ctl is required. Install the v4l-utils package.\n' >&2
  exit 3
fi

mkdir -p "$OUT_DIR"

run_and_save() {
  local filename="$1"
  shift
  {
    printf '$'
    printf ' %q' "$@"
    printf '\n\n'
    "$@"
  } >"${OUT_DIR}/${filename}" 2>&1 || true
}

{
  printf 'Collected: %s\n' "$(date --iso-8601=seconds 2>/dev/null || date)"
  printf 'Device: %s\n' "$DEVICE"
  printf 'Kernel: %s\n' "$(uname -a)"
  if [[ -r /etc/os-release ]]; then
    printf '\n'
    cat /etc/os-release
  fi
} >"${OUT_DIR}/00-system.txt"

run_and_save "10-v4l2-all.txt" v4l2-ctl --device "$DEVICE" --all
run_and_save "11-v4l2-formats-ext.txt" v4l2-ctl --device "$DEVICE" --list-formats-ext
run_and_save "12-v4l2-controls.txt" v4l2-ctl --device "$DEVICE" --list-ctrls-menus
run_and_save "13-v4l2-devices.txt" v4l2-ctl --list-devices

if command -v udevadm >/dev/null 2>&1; then
  run_and_save "20-udev-info.txt" udevadm info --query=all --name "$DEVICE"
fi

if command -v lsusb >/dev/null 2>&1; then
  run_and_save "30-lsusb-tree.txt" lsusb -t
  run_and_save "31-lsusb-summary.txt" lsusb
  run_and_save "32-lsusb-verbose.txt" lsusb -v
fi

if command -v media-ctl >/dev/null 2>&1; then
  run_and_save "40-media-topology.txt" media-ctl -p
fi

if command -v dmesg >/dev/null 2>&1; then
  {
    printf '$ dmesg | relevant camera/USB filter\n\n'
    dmesg 2>&1 | grep -Ei 'uvc|video4linux|v4l2|usb|xhci|ehci|camera|csi|isp' | tail -n 500
  } >"${OUT_DIR}/50-kernel-messages.txt" || true
fi

cat >"${OUT_DIR}/README-REDACT-BEFORE-SHARING.txt" <<'EOF'
Review every file before sharing.

Potentially sensitive fields:
- USB serial numbers
- hostnames and usernames
- product/vendor strings
- internal device paths
- unrelated kernel messages
- customer or product identifiers
EOF

if command -v sha256sum >/dev/null 2>&1; then
  (
    cd "$OUT_DIR" || exit 1
    find . -maxdepth 1 -type f ! -name SHA256SUMS.txt -print0 \
      | sort -z \
      | xargs -0 sha256sum >SHA256SUMS.txt
  )
fi

printf 'Diagnostics written to: %s\n' "$OUT_DIR"
