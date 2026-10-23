"""Exercise the real ncurses executable in a Unix pseudo-terminal."""

import errno
import fcntl
import os
from pathlib import Path
import pty
import select
import signal
import struct
import subprocess
import termios
import time
import unittest

ROOT = Path(__file__).resolve().parent.parent
BINARY = ROOT / "game_of_life"
SEED = ROOT / "patterns" / "glider.txt"


def resize(fd, rows, columns):
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", rows, columns, 0, 0))


class TerminalGame:
    def __init__(self, rows=26, columns=80, terminal="xterm"):
        self.pid, self.fd = pty.fork()
        if self.pid == 0:
            resize(0, rows, columns)
            with SEED.open("rb") as seed:
                os.dup2(seed.fileno(), 0)
            environment = dict(os.environ, TERM=terminal)
            os.execve(str(BINARY), [str(BINARY)], environment)
        self.finished = False
        self.output = bytearray()

    def read_until(self, expected, timeout=5):
        start = len(self.output)
        deadline = time.monotonic() + timeout
        while expected not in self.output[start:] and time.monotonic() < deadline:
            readable, _, _ = select.select([self.fd], [], [], 0.1)
            if readable:
                try:
                    chunk = os.read(self.fd, 65536)
                except OSError as error:
                    if error.errno != errno.EIO:
                        raise
                    chunk = b""
                if not chunk:
                    break
                self.output.extend(chunk)
        if expected not in self.output[start:]:
            raise AssertionError(f"Missing terminal output {expected!r}: {self.output[start:]!r}")

    def send(self, keys):
        os.write(self.fd, keys)

    def finish(self, timeout=5):
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            process, status = os.waitpid(self.pid, os.WNOHANG)
            if process:
                self.finished = True
                return os.waitstatus_to_exitcode(status)
            select.select([], [], [], 0.02)
        raise AssertionError("Game failed to exit")

    def close(self):
        if not self.finished:
            try:
                os.kill(self.pid, signal.SIGKILL)
            except ProcessLookupError:
                pass
            os.waitpid(self.pid, 0)
        os.close(self.fd)


class TerminalTests(unittest.TestCase):
    def test_redirected_input_speed_resize_and_exit(self):
        game = TerminalGame()
        self.addCleanup(game.close)
        game.read_until(b"Delay: 250 ms")
        self.assertIn(b"O", game.output)
        # A resize forces a complete redraw, making the displayed delay observable.
        for key, columns, delay in [(b"a", 81, 200), (b"Z", 82, 250),
                                    (b"A", 83, 200), (b"z", 84, 250)]:
            game.send(key)
            # Allow getch to consume the key before the resize event.
            select.select([], [], [], 0.1)
            resize(game.fd, 26, columns)
            game.read_until(f"Delay: {delay} ms".encode())
        resize(game.fd, 10, 40)
        game.read_until(b"Resize to at least")
        resize(game.fd, 26, 80)
        game.read_until(b"Delay: 250 ms")
        game.send(b" ")
        self.assertEqual(game.finish(), 0)
        mode = termios.tcgetattr(game.fd)[3]
        self.assertTrue(mode & termios.ICANON)
        self.assertTrue(mode & termios.ECHO)

    def test_small_terminal_still_accepts_exit(self):
        game = TerminalGame(rows=10, columns=40)
        self.addCleanup(game.close)
        game.read_until(b"Resize to at least")
        game.send(b" ")
        self.assertEqual(game.finish(), 0)

    def test_unknown_terminal_fails_cleanly(self):
        game = TerminalGame(terminal="school21-terminal-does-not-exist")
        self.addCleanup(game.close)
        game.read_until(b"Cannot initialize ncurses")
        self.assertEqual(game.finish(), 1)

    def test_absent_terminal_fails_cleanly(self):
        result = subprocess.run([str(BINARY)], input=SEED.read_bytes(),
                                capture_output=True, start_new_session=True, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"Cannot initialize ncurses", result.stderr)

    def test_invalid_input_fails_before_terminal_initialization(self):
        result = subprocess.run([str(BINARY)], input=b"invalid\n", capture_output=True,
                                start_new_session=True, timeout=5)
        self.assertEqual(result.returncode, 1)
        self.assertIn(b"Expected exactly 2000 cells", result.stderr)
        self.assertNotIn(b"Cannot initialize ncurses", result.stderr)


if __name__ == "__main__":
    unittest.main(verbosity=2)
