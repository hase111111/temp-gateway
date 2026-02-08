#!/bin/bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${ROOT_DIR}/build"

if [[ -d "${BUILD_DIR}" ]]; then
	rm -rf "${BUILD_DIR}"
	echo "Removed build directory: ${BUILD_DIR}"
else
	echo "No build directory to remove: ${BUILD_DIR}"
fi
