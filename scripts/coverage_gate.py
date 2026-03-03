#!/usr/bin/env python3
import argparse
import json
import sys
from typing import Tuple


def load_json(path: str) -> dict:
    with open(path, 'r', encoding='utf-8') as f:
        return json.load(f)


def metric_values(payload: dict, key: str) -> Tuple[float, int]:
    metric = payload.get(key, {})
    pct = float(metric.get('pct', 0.0))
    total = int(metric.get('total', 0))
    return pct, total


def main() -> int:
    parser = argparse.ArgumentParser(
        description='Fail when current coverage regresses vs baseline.'
    )
    parser.add_argument(
        '--baseline',
        default='tests/coverage/baseline.json',
        help='Baseline summary JSON path (default: tests/coverage/baseline.json)',
    )
    parser.add_argument(
        '--summary',
        default='build/coverage/summary.json',
        help='Current summary JSON path (default: build/coverage/summary.json)',
    )
    parser.add_argument(
        '--epsilon',
        type=float,
        default=0.0001,
        help='Floating-point tolerance for pct comparisons.',
    )
    args = parser.parse_args()

    baseline = load_json(args.baseline)
    current = load_json(args.summary)

    failures = []
    for key in ('lines', 'branches'):
        b_pct, b_total = metric_values(baseline, key)
        c_pct, c_total = metric_values(current, key)

        # If baseline had no measurable data for this metric, do not gate it yet.
        if b_total <= 0:
            print(f'[coverage-gate] {key}: baseline total=0, gate skipped (current={c_pct:.4f}%).')
            continue

        if c_total <= 0:
            failures.append(
                f'{key}: current total is 0 but baseline total is {b_total}'
            )
            continue

        print(
            f'[coverage-gate] {key}: baseline={b_pct:.4f}% current={c_pct:.4f}% '
            f'(baseline total={b_total}, current total={c_total})'
        )
        if c_pct + args.epsilon < b_pct:
            failures.append(
                f'{key}: regression detected (baseline {b_pct:.4f}% > current {c_pct:.4f}%)'
            )

    if failures:
        print('[coverage-gate] FAILED')
        for failure in failures:
            print(f'  - {failure}')
        return 1

    print('[coverage-gate] PASSED')
    return 0


if __name__ == '__main__':
    sys.exit(main())
