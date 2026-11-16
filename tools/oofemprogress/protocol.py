"""Reader of the ``oofem-progress`` protocol.

OOFEM's ``progress`` export module (SolutionProgressExportModule) writes a JSON Lines file:
one self-contained JSON record per line, flushed after every record. Record types:

* ``header`` - job identification (OOFEM version, input, host, pid, start time) and the
  description of the monitored channels and suggested charts,
* ``iter``   - one nonlinear solver iteration (error norms per DOF group),
* ``step``   - one solution step (time, solver status, values of the channels),
* ``end``    - normal termination of the analysis.

Unknown record types and fields are ignored, so that newer writers remain readable.
This module uses the standard library only.
"""

import json
import os
import socket
import sys
import time

PROTOCOL = "oofem-progress"
SUPPORTED_VERSION = 1

#: Series that are always available (not configured as channels).
BUILTIN_SERIES = ("time", "step", "dt", "wall", "iters", "attempts")

#: Synthetic record emitted by :class:`ProgressReader` when the file was restarted (new run).
RESET = {"type": "_reset"}


class ProgressReader:
    """Incrementally reads new records from a progress file.

    The file is opened, read from the last position and closed again on every :meth:`poll`.
    No handle is kept open between polls, so the solver can truncate or replace the file
    (which would fail on Windows while another process holds it open).

    A new run is detected by the file becoming shorter or by a change of its first line
    (the header); a :data:`RESET` record is then returned before the records of the new run.
    """

    #: Maximum number of bytes read in one poll (protects against huge files on first read).
    CHUNK = 16 * 1024 * 1024

    def __init__(self, path):
        self.path = path
        self.pos = 0
        self.buffer = b""
        self.first_line = None
        self.bad_lines = 0

    def exists(self):
        return os.path.exists(self.path)

    def _restart(self):
        self.pos = 0
        self.buffer = b""
        self.first_line = None

    def poll(self):
        """Returns the list of records (dicts) appended since the last call."""
        try:
            f = open(self.path, "rb")
        except OSError:
            return []
        records = []
        with f:
            f.seek(0, os.SEEK_END)
            size = f.tell()
            restarted = size < self.pos
            if not restarted and self.first_line is not None:
                f.seek(0)
                restarted = f.read(len(self.first_line)) != self.first_line
            if restarted:
                self._restart()
                records.append(dict(RESET))
            f.seek(self.pos)
            data = f.read(self.CHUNK)
        self.pos += len(data)
        self.buffer += data
        *lines, self.buffer = self.buffer.split(b"\n")  # last item is an incomplete line (or b"")
        for line in lines:
            if self.first_line is None:
                self.first_line = line + b"\n"
            line = line.strip()  # also removes \r of files written in text mode on Windows
            if not line:
                continue
            try:
                rec = json.loads(line.decode("utf-8", errors="replace"))
            except ValueError:
                self.bad_lines += 1
                continue
            if isinstance(rec, dict):
                records.append(rec)
        return records


def is_process_alive(pid, host=None):
    """Checks whether the solver process is still running.

    Returns True/False, or None if it cannot be determined (e.g. the solver runs on another host).
    Never uses ``os.kill(pid, 0)`` on Windows, where it would terminate the process.
    """
    if not pid:
        return None
    if host and not _same_host(host):
        return None
    if sys.platform == "win32":
        return _win_process_alive(int(pid))
    try:
        os.kill(int(pid), 0)
    except ProcessLookupError:
        return False
    except PermissionError:
        return True  # exists, owned by another user
    except OSError:
        return None
    return True


def _same_host(host):
    def short(name):
        return name.split(".")[0].lower()
    return short(host) == short(socket.gethostname())


def _win_process_alive(pid):
    import ctypes
    from ctypes import wintypes
    PROCESS_QUERY_LIMITED_INFORMATION = 0x1000
    STILL_ACTIVE = 259
    kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
    kernel32.OpenProcess.restype = wintypes.HANDLE
    kernel32.OpenProcess.argtypes = (wintypes.DWORD, wintypes.BOOL, wintypes.DWORD)
    kernel32.GetExitCodeProcess.argtypes = (wintypes.HANDLE, ctypes.POINTER(wintypes.DWORD))
    kernel32.CloseHandle.argtypes = (wintypes.HANDLE,)
    handle = kernel32.OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, False, pid)
    if not handle:
        # access denied means the process exists; anything else (invalid parameter) means it does not
        return ctypes.get_last_error() == 5
    try:
        code = wintypes.DWORD()
        if not kernel32.GetExitCodeProcess(handle, ctypes.byref(code)):
            return None
        return code.value == STILL_ACTIVE
    finally:
        kernel32.CloseHandle(handle)


class RunState:
    """Accumulated state of one run, built by applying records in order."""

    def __init__(self):
        self.reset()

    def reset(self):
        self.header = None
        self.end = None
        self.channels = []           # channel descriptions from the header
        self.series = {}             # series name -> list of values (None = not available)
        self.iterations = {}         # (step, attempt) -> list of iter records
        self.last_iter_key = None
        self.last_step = None        # last step record
        self.last_record_clock = None  # time.monotonic() when the last record arrived
        self.version_warning = None
        self.revision = 0            # incremented on every change (frontends redraw on change)

    # ------------------------------------------------------------------ records
    def apply(self, rec):
        kind = rec.get("type")
        if kind == "_reset":
            self.reset()
        elif kind == "header":
            self.reset()
            self.header = rec
            self.channels = list(rec.get("channels") or [])
            for name in self.series_names():
                self.series[name] = []
            if rec.get("protocol") != PROTOCOL:
                self.version_warning = "unknown protocol %r" % rec.get("protocol")
            elif (rec.get("version") or 0) > SUPPORTED_VERSION:
                self.version_warning = "protocol version %s is newer than supported %d" % (
                    rec.get("version"), SUPPORTED_VERSION)
        elif kind == "step":
            self._apply_step(rec)
        elif kind == "iter":
            key = (rec.get("step"), rec.get("attempt", 1))
            self.iterations.setdefault(key, []).append(rec)
            self.last_iter_key = key
        elif kind == "end":
            self.end = rec
        else:
            return  # unknown record type, ignored
        self.last_record_clock = time.monotonic()
        self.revision += 1

    def apply_all(self, records):
        for rec in records:
            self.apply(rec)

    def _apply_step(self, rec):
        if self.header is None:
            return
        self.last_step = rec
        solver = rec.get("solver") or {}
        builtin = {
            "time": rec.get("time"),
            "step": rec.get("step"),
            "dt": rec.get("dt"),
            "wall": rec.get("wall"),
            "iters": solver.get("iters"),
            "attempts": solver.get("attempts"),
        }
        for name, value in builtin.items():
            self.series.setdefault(name, []).append(value)
        values = rec.get("values") or []
        for i, ch in enumerate(self.channels):
            value = values[i] if i < len(values) else None
            self.series.setdefault(ch["id"], []).append(value)

    # ------------------------------------------------------------------ queries
    def series_names(self):
        """Names of all plottable series: built-ins followed by the channels."""
        return list(BUILTIN_SERIES) + [ch["id"] for ch in self.channels]

    def channel(self, name):
        for ch in self.channels:
            if ch.get("id") == name:
                return ch
        return None

    def nsteps_done(self):
        return len(self.series.get("step", []))

    def status(self):
        """One of: waiting, running, finished, crashed, unknown."""
        if self.header is None:
            return "waiting"
        if self.end is not None:
            return self.end.get("status", "finished")
        alive = is_process_alive(self.header.get("pid"), self.header.get("host"))
        if alive is False:
            return "crashed"
        if alive is None:
            return "unknown"
        return "running"

    def progress(self):
        """Fraction of the analysis done (0..1) or None if unknown."""
        if self.header is None:
            return None
        if self.end is not None:
            return 1.0
        if self.last_step is None:
            return 0.0
        final_time = self.header.get("final_time")
        t = self.last_step.get("time")
        if final_time and t is not None and final_time > 0:
            return max(0.0, min(1.0, t / final_time))
        nsteps = self.header.get("nsteps")
        if nsteps:
            return max(0.0, min(1.0, self.last_step.get("step", 0) / nsteps))
        return None

    def elapsed(self):
        """Wall clock time of the solver in seconds (estimated between records while running)."""
        if self.end is not None:
            return self.end.get("wall")
        last = None
        if self.last_iter_key is not None:
            last = self.iterations[self.last_iter_key][-1].get("wall")
        if self.last_step is not None:
            w = self.last_step.get("wall")
            if w is not None and (last is None or w > last):
                last = w
        if last is None:
            return None
        if self.last_record_clock is not None and self.status() == "running":
            last += time.monotonic() - self.last_record_clock
        return last

    def info(self):
        """Summary used by the info panel of the viewers."""
        h = self.header or {}
        step = self.last_step or {}
        solver = step.get("solver") or {}
        return {
            "job": h.get("job"),
            "input": h.get("input"),
            "description": h.get("description"),
            "oofem": h.get("oofem"),
            "git": h.get("git"),
            "host": h.get("host"),
            "pid": h.get("pid"),
            "engngmodel": h.get("engngmodel"),
            "start": h.get("start"),
            "nsteps": h.get("nsteps"),
            "final_time": h.get("final_time"),
            "step": step.get("step"),
            "time": step.get("time"),
            "solver_status": solver.get("status"),
            "iters": solver.get("iters"),
            "attempts": solver.get("attempts"),
            "status": self.status(),
            "progress": self.progress(),
            "elapsed": self.elapsed(),
            "warning": self.version_warning,
            "end": self.end,
        }


def format_duration(seconds):
    if seconds is None:
        return "-"
    seconds = int(seconds)
    h, rem = divmod(seconds, 3600)
    m, s = divmod(rem, 60)
    return "%d:%02d:%02d" % (h, m, s)
