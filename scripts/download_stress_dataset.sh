#!/bin/bash

# ==============================================================================
# CGalleryOrganizer Stress Dataset Downloader
# ==============================================================================
# This script automates the downloading of a massive open-source dataset containing
# raw smartphone photos with intact EXIF metadata for stress testing the Exiv2
# integration and caching engine.
#
# Target Dataset: "Low Light Smartphone Images (Raw format)" via Kaggle
# Size: ~3.7 GB
# ==============================================================================

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
STRESS_DIR="${BUILD_DIR}/stress_data"
LOCKFILE="${STRESS_DIR}/.downloaded"

echo "[*] Initializing Stress Dataset Download..."

# 1. Ensure Kaggle CLI is installed
if ! command -v kaggle &> /dev/null; then
    echo "ERROR: Kaggle CLI could not be found."
    echo ""
    echo "This dataset requires the Kaggle CLI due to its size."
    echo "Please install it using: 'pip install kaggle'"
    echo "And ensure you have your ~/.kaggle/kaggle.json API token configured."
    echo "Guide: https://www.kaggle.com/docs/api"
    exit 1
fi

# 2. Check if already downloaded
if [ -f "$LOCKFILE" ]; then
    echo "[+] Dataset is already present in ${STRESS_DIR}. Skipping download."
    exit 0
fi

# 3. Create target directory
mkdir -p "$STRESS_DIR"

# 4. Download and unzip directly into the stress directory
echo "[*] Downloading 'crkaushik/low-light-smartphone-images-raw-format' from Kaggle..."
echo "[*] This is a multi-gigabyte dataset. Please be patient."
kaggle datasets download -d crkaushik/low-light-smartphone-images-raw-format --unzip -p "$STRESS_DIR"

# 5. Lock the directory to prevent redundant downloads
touch "$LOCKFILE"

echo ""
echo "[+] SUCCESS: Dataset downloaded and extracted successfully to:"
echo "    ${STRESS_DIR}"
echo ""
echo "    You can now run 'make stress' to benchmark the Exiv2 parser against this library."
