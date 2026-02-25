#!/bin/bash

set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
MANIFEST_PATH="${REPO_ROOT}/models/manifest.json"
TARGET_DIR="${REPO_ROOT}/build/models"
LOCKFILE="${TARGET_DIR}/.installed.json"

if [ ! -f "${MANIFEST_PATH}" ]; then
  echo "ERROR: Manifest not found at ${MANIFEST_PATH}" >&2
  exit 1
fi

for tool in curl shasum python3 base64; do
  if ! command -v "$tool" >/dev/null 2>&1; then
    echo "ERROR: required tool '$tool' is missing." >&2
    exit 1
  fi
done

mkdir -p "${TARGET_DIR}"

entries_file="$(mktemp)"
trap 'rm -f "${entries_file}"' EXIT

python3 - "${MANIFEST_PATH}" >"${entries_file}" <<'PY'
import json
import re
import sys

manifest_path = sys.argv[1]
required = [
    "id",
    "task",
    "url",
    "sha256",
    "license_name",
    "license_url",
    "author",
    "source_url",
    "credit_text",
    "version",
    "filename",
]
allowed_tasks = {"classification", "text_detection", "embedding"}
sha_pattern = re.compile(r"^[a-fA-F0-9]{64}$")

with open(manifest_path, "r", encoding="utf-8") as f:
    manifest = json.load(f)

if "models" not in manifest or not isinstance(manifest["models"], list):
    print("ERROR\tmanifest must contain a 'models' list", flush=True)
    sys.exit(2)

for idx, model in enumerate(manifest["models"]):
    if not isinstance(model, dict):
        print(f"ERROR\tmodels[{idx}] must be an object", flush=True)
        sys.exit(2)

    for field in required:
      if field not in model or str(model[field]).strip() == "":
        print(f"ERROR\tmissing required field '{field}' in models[{idx}]", flush=True)
        sys.exit(2)

    if model["task"] not in allowed_tasks:
      print(f"ERROR\tinvalid task '{model['task']}' in models[{idx}]", flush=True)
      sys.exit(2)

    if not sha_pattern.match(model["sha256"]):
      print(f"ERROR\tinvalid sha256 '{model['sha256']}' in models[{idx}]", flush=True)
      sys.exit(2)

    print(
      "OK\t{id}\t{task}\t{url}\t{sha}\t{filename}\t{license_name}\t{author}\t{source_url}\t{version}".format(
        id=model["id"],
        task=model["task"],
        url=model["url"],
        sha=model["sha256"],
        filename=model["filename"],
        license_name=model["license_name"],
        author=model["author"],
        source_url=model["source_url"],
        version=model["version"],
      ),
      flush=True,
    )
PY

if grep -q '^ERROR' "${entries_file}"; then
  cat "${entries_file}" >&2
  exit 2
fi

installed_meta="$(mktemp)"
trap 'rm -f "${entries_file}" "${installed_meta}"' EXIT

echo "[*] Installing models into ${TARGET_DIR}"

while IFS=$'\t' read -r status id task url sha filename license author source version; do
  [ -z "${status}" ] && continue
  if [ "${status}" != "OK" ]; then
    continue
  fi

  out_path="${TARGET_DIR}/${filename}"
  echo "  - ${id} (${task})"
  if [[ "${url}" == data:application/octet-stream\;base64,* ]]; then
    payload="${url#data:application/octet-stream;base64,}"
    printf '%s' "${payload}" | base64 --decode >"${out_path}"
  else
    curl -fsSL "${url}" -o "${out_path}"
  fi

  actual_sha="$(shasum -a 256 "${out_path}" | awk '{print $1}')"
  if [ "${actual_sha}" != "${sha}" ]; then
    echo "ERROR: SHA256 mismatch for ${id}. expected=${sha} actual=${actual_sha}" >&2
    rm -f "${out_path}"
    exit 3
  fi

  printf '%s\t%s\t%s\t%s\t%s\t%s\n' "${id}" "${task}" "${filename}" "${sha}" "${license}" "${version}" >>"${installed_meta}"
done <"${entries_file}"

python3 - "${installed_meta}" "${LOCKFILE}" <<'PY'
import json
import sys
from datetime import datetime, timezone

installed_file = sys.argv[1]
lockfile = sys.argv[2]

models = []
with open(installed_file, "r", encoding="utf-8") as f:
  for line in f:
    line = line.strip()
    if not line:
      continue
    model_id, task, filename, sha, license_name, version = line.split("\t")
    models.append(
      {
        "id": model_id,
        "task": task,
        "filename": filename,
        "sha256": sha,
        "license_name": license_name,
        "version": version,
      }
    )

payload = {
  "installed_at": datetime.now(timezone.utc).isoformat(),
  "models": models,
}

with open(lockfile, "w", encoding="utf-8") as f:
  json.dump(payload, f, indent=2)
PY

echo "[+] Model installation complete."
echo "[+] Lockfile written to ${LOCKFILE}"
