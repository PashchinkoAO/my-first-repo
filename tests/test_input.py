#!/usr/bin/env python3
"""Check the input contract through the C test executable, without a terminal."""

import subprocess
import sys
from pathlib import Path


def check_input(executable, data, expected_alive):
    result = subprocess.run(
        [str(executable), "--read-field"],
        input=data,
        capture_output=True,
        timeout=5,
        check=False,
    )
    expected_status = 0 if expected_alive is not None else 1
    assert result.returncode == expected_status, (result.returncode, result.stderr)
    if expected_alive is not None:
        assert result.stdout == f"alive={expected_alive}\n".encode(), result.stdout
    else:
        assert result.stdout == b"", result.stdout
    assert result.stderr == b"", result.stderr


def main():
    root = Path(__file__).resolve().parent.parent
    executable = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else root / "test_game_of_life"
    seeds = {"block": 4, "blinker": 3, "beacon": 8, "glider": 5, "lwss": 9}
    for name, expected_alive in seeds.items():
        data = (root / "patterns" / f"{name}.txt").read_bytes()
        rows = data.splitlines()
        assert len(rows) == 25 and all(len(row) == 80 for row in rows), name
        assert set(data) <= {ord("0"), ord("1"), ord("\n")}, name
        check_input(executable, data, expected_alive)

    valid_cases = [
        (b"0" * 2000, 0),
        (b"1" * 2000, 2000),
        (b"0 \t1\n" * 1000, 1000),
        ((root / "patterns/glider.txt").read_bytes().replace(b"\n", b"\r\n"), 5),
    ]
    for data, expected_alive in valid_cases:
        check_input(executable, data, expected_alive)

    invalid_cases = [
        b"",
        b"0" * 1999,
        b"0" * 2001,
        b"x" + b"0" * 1999,
        b"0" * 2000 + b"x",
        b"0" * 1000 + b"2" + b"0" * 999,
    ]
    for data in invalid_cases:
        check_input(executable, data, None)
    print("PASS: 15 input checks (5 seeds, whitespace/CRLF, invalid and incomplete input).")


if __name__ == "__main__":
    main()
