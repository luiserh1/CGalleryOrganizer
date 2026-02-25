#!/bin/bash

# ==============================================================================
# CGalleryOrganizer Stress Dataset Downloader
# ==============================================================================
# This script automates the downloading of a massive open-source dataset containing
# raw smartphone photos with intact EXIF metadata for stress testing the Exiv2
# integration and caching engine.
#
# Target Dataset: "COCO 2017" (Common Objects in Context)
# Options: Validation (~815MB, 5k images) or Training (~19GB, 118k images)
# ==============================================================================

set -e

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${REPO_ROOT}/build"
STRESS_DIR="${BUILD_DIR}/stress_data"
LOCKFILE="${STRESS_DIR}/.downloaded"

echo "[*] Initializing Stress Dataset Download..."

# 1. Check for required tools
for tool in curl unzip; do
    if ! command -v "$tool" &> /dev/null; then
        echo "ERROR: $tool could not be found. Please install it."
        exit 1
    fi
done

# 2. Check if already downloaded
if [ -f "$LOCKFILE" ]; then
    echo "[+] Dataset is already present in ${STRESS_DIR}. Skipping download."
    exit 0
fi

# 3. Parse options
DATASET_URL="http://images.cocodataset.org/zips/val2017.zip"
DATASET_NAME="COCO 2017 Validation"

if [ "$1" == "--heavy" ]; then
    DATASET_URL="http://images.cocodataset.org/zips/train2017.zip"
    DATASET_NAME="COCO 2017 Training (EXTREME)"
fi

# 4. Create target directory
mkdir -p "$STRESS_DIR"

# 5. Download and unzip directly into the stress directory
echo "[*] Downloading '${DATASET_NAME}' from Amazon S3..."
echo "[*] This is a multi-gigabyte dataset. Please be patient."

TEMP_ZIP="${STRESS_DIR}/temp_dataset.zip"
curl -L "$DATASET_URL" -o "$TEMP_ZIP"

echo "[*] Extracting images..."
unzip -q "$TEMP_ZIP" -d "$STRESS_DIR"
rm "$TEMP_ZIP"

# Note: COCO unzips into val2017/ or train2017/, let's flatten it
if [ -d "${STRESS_DIR}/val2017" ]; then
    mv "${STRESS_DIR}/val2017/"* "$STRESS_DIR/"
    rmdir "${STRESS_DIR}/val2017"
elif [ -d "${STRESS_DIR}/train2017" ]; then
    mv "${STRESS_DIR}/train2017/"* "$STRESS_DIR/"
    rmdir "${STRESS_DIR}/train2017"
fi

# 5. Lock the directory to prevent redundant downloads
touch "$LOCKFILE"

echo ""
echo "[+] SUCCESS: Dataset downloaded and extracted successfully to:"
echo "    ${STRESS_DIR}"
echo ""
echo "    You can now run 'make stress' to benchmark the Exiv2 parser against this library."
