#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BIN="${ROOT_DIR}/build/gateway"

if [[ ! -x "${BIN}" ]]; then
  echo "Executable not found: ${BIN}" >&2
  echo "Build it first: ${ROOT_DIR}/bash/build.sh" >&2
  exit 1
fi

"${BIN}"
