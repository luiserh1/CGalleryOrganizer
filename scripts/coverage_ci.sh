#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
COVERAGE_DIR="$ROOT_DIR/build/coverage"

CC_BIN="${CC:-gcc}"
CXX_BIN="${CXX:-g++}"
EXTRA_CFLAGS_VAL="${EXTRA_CFLAGS:---coverage -O0 -Wno-error}"
EXTRA_CXXFLAGS_VAL="${EXTRA_CXXFLAGS:---coverage -O0 -Wno-error}"
EXTRA_LDFLAGS_VAL="${EXTRA_LDFLAGS:---coverage}"

if ! command -v gcovr >/dev/null 2>&1; then
  echo "Error: gcovr is required. Install with: python3 -m pip install --user gcovr" >&2
  exit 2
fi

mkdir -p "$COVERAGE_DIR"
rm -f "$COVERAGE_DIR"/coverage.txt "$COVERAGE_DIR"/coverage.json "$COVERAGE_DIR"/coverage.xml "$COVERAGE_DIR"/summary.json

cd "$ROOT_DIR"

make clean
make test \
  CC="$CC_BIN" \
  CXX="$CXX_BIN" \
  EXTRA_CFLAGS="$EXTRA_CFLAGS_VAL" \
  EXTRA_CXXFLAGS="$EXTRA_CXXFLAGS_VAL" \
  EXTRA_LDFLAGS="$EXTRA_LDFLAGS_VAL"

GCOVR_ARGS=(
  --root "$ROOT_DIR"
  --filter "$ROOT_DIR/src"
  --exclude "$ROOT_DIR/tests"
  --exclude "$ROOT_DIR/vendor"
  --exclude "$ROOT_DIR/build"
)

gcovr "${GCOVR_ARGS[@]}" --txt "$COVERAGE_DIR/coverage.txt"
gcovr "${GCOVR_ARGS[@]}" --json "$COVERAGE_DIR/coverage.json"
gcovr "${GCOVR_ARGS[@]}" --xml-pretty "$COVERAGE_DIR/coverage.xml"

python3 - "$COVERAGE_DIR/coverage.json" "$COVERAGE_DIR/summary.json" <<'PY'
import json
import sys
from datetime import datetime, timezone

coverage_json_path = sys.argv[1]
summary_path = sys.argv[2]

with open(coverage_json_path, 'r', encoding='utf-8') as f:
    data = json.load(f)

covered_lines = 0
total_lines = 0
covered_branches = 0
total_branches = 0

for file_item in data.get('files', []):
    for line in file_item.get('lines', []):
        if line.get('gcovr/noncode', False):
            continue
        count = line.get('count', 0)
        total_lines += 1
        if isinstance(count, int) and count > 0:
            covered_lines += 1

        for branch in line.get('branches', []):
            bcount = branch.get('count', 0)
            total_branches += 1
            if isinstance(bcount, int) and bcount > 0:
                covered_branches += 1

def pct(covered, total):
    if total <= 0:
        return 0.0
    return round((covered * 100.0) / total, 4)

summary = {
    'version': 1,
    'generatedAtUtc': datetime.now(timezone.utc).strftime('%Y-%m-%dT%H:%M:%SZ'),
    'filesAnalyzed': len(data.get('files', [])),
    'lines': {
        'covered': covered_lines,
        'total': total_lines,
        'pct': pct(covered_lines, total_lines),
    },
    'branches': {
        'covered': covered_branches,
        'total': total_branches,
        'pct': pct(covered_branches, total_branches),
    },
}

with open(summary_path, 'w', encoding='utf-8') as f:
    json.dump(summary, f, indent=2, ensure_ascii=True)

print(json.dumps(summary, indent=2, ensure_ascii=True))
PY

echo "Coverage artifacts generated under: $COVERAGE_DIR"
