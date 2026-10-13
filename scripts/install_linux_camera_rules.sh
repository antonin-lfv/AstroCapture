#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" != Linux ]]; then
    echo "Ce réglage est réservé à Linux." >&2
    exit 1
fi
if [[ "$EUID" -ne 0 ]]; then
    echo "Relancez avec sudo pour autoriser les caméras USB." >&2
    exit 1
fi
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
RULES="${SCRIPT_DIR}/../packaging/linux/70-astrocapture-cameras.rules"
if [[ ! -f "$RULES" ]]; then
    RULES="${SCRIPT_DIR}/70-astrocapture-cameras.rules"
fi
install -m 644 "$RULES" /etc/udev/rules.d/70-astrocapture-cameras.rules
udevadm control --reload-rules
echo "Autorisations installées. Débranchez puis rebranchez la caméra."
