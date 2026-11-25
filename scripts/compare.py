#!/usr/bin/env python3
import os
import sys

import numpy as np


def read_pgm(path):
    with open(path, "rb") as f:
        magic, size, maxval, data = f.read().split(b"\n", 3)
    w, h = map(int, size.split())
    if magic != b"P5" or maxval != b"255":
        raise ValueError(f"{path}: bukan PGM P5 8-bit")
    return np.frombuffer(data, dtype=np.uint8, count=w * h).reshape(h, w)


def main():
    ref_dir, test_dir, n, name = sys.argv[1], sys.argv[2], int(sys.argv[3]), sys.argv[4]
    identical = 0
    missing = 0
    abs_sum = 0.0
    signed_sum = 0.0
    max_diff = 0
    pixels = 0

    for i in range(1, n + 1):
        fname = f"edge_{i:04d}.pgm"
        test_path = os.path.join(test_dir, fname)
        if not os.path.exists(test_path):
            missing += 1
            continue
        ref = read_pgm(os.path.join(ref_dir, fname)).astype(np.int32)
        test = read_pgm(test_path).astype(np.int32)
        diff = test - ref
        if not diff.any():
            identical += 1
        abs_sum += np.abs(diff).sum()
        signed_sum += diff.sum()
        max_diff = max(max_diff, int(np.abs(diff).max()))
        pixels += diff.size

    found = n - missing
    print(f"  {name} vs V0: {found}/{n} frame ditemukan, {identical}/{n} frame identik")
    if pixels:
        print(f"    selisih rata-rata magnitudo |{name} - V0| = {abs_sum / pixels:.3f}"
              f" (rata-rata bertanda {signed_sum / pixels:+.3f}, maks {max_diff}) dari skala 0-255")
    if name == "V1":
        print("    ->", "OK: identik dengan V0" if identical == n else "GAGAL: berbeda dari V0")


if __name__ == "__main__":
    main()
