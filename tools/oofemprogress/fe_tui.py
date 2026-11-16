"""Terminal frontend using the standard library only (ANSI escape sequences, no curses).

Works on Linux/macOS terminals and on Windows 10/11 (Windows Terminal and the legacy console;
virtual terminal processing is enabled through ctypes). Charts are drawn by
:mod:`.tui_canvas` (braille or ASCII) or optionally by plotext.

Keys: a=add chart, c=add convergence chart, d=delete chart, n/p (or arrows)=next/previous chart,
g=toggle grid of all charts, s=save layout, q=quit.
"""

import heapq
import itertools
import math
import os
import shutil
import sys
import time

from .frontend import Frontend, ChartView
from .charts import ChartSpec, parse_chart_option
from .protocol import format_duration
from . import tui_canvas

IS_WINDOWS = sys.platform == "win32"

STATUS_COLORS = {"running": 32, "finished": 36, "crashed": 31, "unknown": 33, "waiting": 33, "no file": 33}
HELP = "a:add  c:conv  d:delete  n/p:next/prev  g:grid  s:save layout  q:quit"


class TuiChartView(ChartView):
    def __init__(self, frontend):
        self.frontend = frontend
        self.spec = None
        self.curves = []
        self.title = ""

    def update(self, spec, curves, title):
        self.spec, self.curves, self.title = spec, curves, title
        self.frontend.dirty = True


class _Keyboard:
    """Non-blocking single key input (cbreak mode on POSIX, msvcrt on Windows)."""

    def __init__(self):
        self.enabled = sys.stdin is not None and sys.stdin.isatty()
        self.saved = None

    def start(self):
        if not self.enabled or IS_WINDOWS:
            return
        import termios
        import tty
        fd = sys.stdin.fileno()
        self.saved = termios.tcgetattr(fd)
        tty.setcbreak(fd)

    def stop(self):
        if self.saved is not None:
            import termios
            termios.tcsetattr(sys.stdin.fileno(), termios.TCSADRAIN, self.saved)
            self.saved = None

    def read(self, timeout):
        """Returns a key string or None after timeout seconds."""
        if not self.enabled:
            time.sleep(max(0.0, timeout))
            return None
        if IS_WINDOWS:
            import msvcrt
            end = time.monotonic() + timeout
            while True:
                if msvcrt.kbhit():
                    ch = msvcrt.getwch()
                    if ch in ("\x00", "\xe0"):  # arrow and function keys
                        code = msvcrt.getwch()
                        return {"K": "left", "M": "right"}.get(code)
                    return ch
                if time.monotonic() >= end:
                    return None
                time.sleep(0.03)
        import select
        ready, _, _ = select.select([sys.stdin], [], [], max(0.0, timeout))
        if not ready:
            return None
        data = os.read(sys.stdin.fileno(), 32).decode(errors="ignore")
        if data.startswith("\x1b["):
            return {"\x1b[D": "left", "\x1b[C": "right"}.get(data[:3])
        return data[:1] if data else None


def _enable_windows_vt():
    """Enables ANSI escape processing in the Windows console (no-op elsewhere)."""
    if not IS_WINDOWS:
        return
    try:
        import ctypes
        kernel32 = ctypes.windll.kernel32
        handle = kernel32.GetStdHandle(-11)  # STD_OUTPUT_HANDLE
        mode = ctypes.c_uint32()
        if kernel32.GetConsoleMode(handle, ctypes.byref(mode)):
            kernel32.SetConsoleMode(handle, mode.value | 0x0004)  # ENABLE_VIRTUAL_TERMINAL_PROCESSING
    except Exception:
        pass


class TuiFrontend(Frontend):
    name = "tui"

    def __init__(self, renderer="braille", color=None, **_options):
        self.renderer = renderer
        self.color = color
        self.views = {}
        self.order = []
        self.current = 0
        self.grid = False
        self.info = {}
        self.msg = ""
        self.timers = []
        self._seq = itertools.count()
        self.dirty = True
        self.running = False
        self.keyboard = _Keyboard()
        self._size = None
        self._render = None

    # ------------------------------------------------------------------ Frontend interface
    def build(self, controller):
        super().build(controller)
        _enable_windows_vt()
        if hasattr(sys.stdout, "reconfigure"):
            try:
                # braille and box characters; Windows consoles may default to a legacy code page
                sys.stdout.reconfigure(encoding="utf-8", errors="replace")
            except Exception:
                pass
        if self.color is None:
            self.color = sys.stdout.isatty() and "NO_COLOR" not in os.environ
        self._render = self._make_renderer()

    def _make_renderer(self):
        if self.renderer == "plotext":
            try:
                from .tui_plotext import render_chart
                return render_chart
            except ImportError:
                self.msg = "plotext not available, using braille renderer"
                self.renderer = "braille"
        mode = "ascii" if self.renderer == "ascii" else "braille"

        def render(curves, width, height, title, xlabel, logx, logy, use_color):
            return tui_canvas.render_chart(curves, width, height, title, xlabel, logx, logy, mode, use_color)
        return render

    def set_info(self, info):
        self.info = info
        self.dirty = True

    def add_chart_view(self, spec):
        view = TuiChartView(self)
        self.views[spec.id] = view
        self.order.append(spec.id)
        self.dirty = True
        return view

    def remove_chart_view(self, chart_id):
        self.views.pop(chart_id, None)
        if chart_id in self.order:
            self.order.remove(chart_id)
        self.current = min(self.current, max(0, len(self.order) - 1))
        self.dirty = True

    def schedule(self, ms, callback):
        heapq.heappush(self.timers, (time.monotonic() + ms / 1000.0, next(self._seq), callback))

    def message(self, text):
        self.msg = text
        self.dirty = True

    def run(self):
        self.running = True
        out = sys.stdout
        out.write("\x1b[?1049h\x1b[?25l")  # alternate screen, hide cursor
        out.flush()
        self.keyboard.start()
        try:
            while self.running:
                timeout = 0.25
                if self.timers:
                    timeout = min(timeout, max(0.0, self.timers[0][0] - time.monotonic()))
                key = self.keyboard.read(timeout)
                if key:
                    self.handle_key(key)
                while self.timers and self.timers[0][0] <= time.monotonic():
                    _, _, callback = heapq.heappop(self.timers)
                    callback()
                size = shutil.get_terminal_size()
                if size != self._size:
                    self._size = size
                    self.dirty = True
                if self.dirty:
                    self.draw()
        except KeyboardInterrupt:
            pass
        finally:
            self.keyboard.stop()
            out.write("\x1b[0m\x1b[?25h\x1b[?1049l")  # restore screen and cursor
            out.flush()

    # ------------------------------------------------------------------ keys
    def handle_key(self, key):
        n = len(self.order)
        if key in ("q", "Q", "\x1b"):
            self.running = False
        elif key in ("n", "right", " ") and n:
            self.current = (self.current + 1) % n
        elif key in ("p", "left") and n:
            self.current = (self.current - 1) % n
        elif key == "g":
            self.grid = not self.grid
        elif key == "c":
            self.controller.add_convergence_chart()
            self.current = len(self.order) - 1
        elif key == "d" and n:
            self.controller.remove_chart(self.order[self.current])
        elif key == "a":
            self.prompt_add_chart()
        elif key == "s":
            self.prompt_save_layout()
        self.dirty = True

    def _prompt(self, lines, question):
        """Shows lines and asks a question in normal (cooked) terminal mode."""
        self.keyboard.stop()
        out = sys.stdout
        out.write("\x1b[H\x1b[2J\x1b[?25h")
        out.write("\n".join(lines) + "\n")
        out.flush()
        try:
            answer = input(question)
        except (EOFError, KeyboardInterrupt):
            answer = ""
        out.write("\x1b[?25l")
        self.keyboard.start()
        self.dirty = True
        return answer.strip()

    def prompt_add_chart(self):
        names = self.controller.series_names()
        cols = max(1, shutil.get_terminal_size().columns // 32)
        lines = ["Add chart - available series:", ""]
        cells = ["%3d  %s" % (i + 1, name) for i, name in enumerate(names)]
        for i in range(0, len(cells), cols):
            lines.append("".join(c[:31].ljust(32) for c in cells[i:i + cols]))
        lines += ["", "Enter X:Y[,Y2...] using numbers or names, optionally followed by 'logx'/'logy',",
                  "or 'conv' for the convergence chart. Empty input cancels."]
        answer = self._prompt(lines, "> ")
        if not answer:
            return
        words = answer.split()
        flags = {w.lower() for w in words[1:]}
        try:
            spec = parse_chart_option(words[0])

            def resolve(token):
                if token.isdigit() and 1 <= int(token) <= len(names):
                    return names[int(token) - 1]
                return token
            if spec.kind == "xy":
                spec = ChartSpec(x=resolve(spec.x), y=[resolve(y) for y in spec.y],
                                 logx="logx" in flags, logy="logy" in flags)
            self.controller.add_chart(spec)
            self.current = len(self.order) - 1
        except ValueError as e:
            self.message(str(e))

    def prompt_save_layout(self):
        default = "progress-layout.json"
        answer = self._prompt(["Save chart layout"], "file name [%s]: " % default)
        try:
            self.controller.save_layout(answer or default)
        except OSError as e:
            self.message("cannot save layout: %s" % e)

    # ------------------------------------------------------------------ drawing
    def _c(self, code, text):
        return "\x1b[%dm%s\x1b[0m" % (code, text) if self.color else text

    def header_lines(self, width):
        info = self.info
        status = info.get("status") or "-"
        step = info.get("step")
        nsteps = info.get("nsteps")
        t, tf = info.get("time"), info.get("final_time")
        parts = [
            self._c(1, "OOFEM progress"),
            "job: %s" % (info.get("job") or "-"),
            self._c(STATUS_COLORS.get(status, 37), status.upper()),
            "step %s/%s" % ("-" if step is None else step, nsteps or "?"),
            "t=%s/%s" % ("-" if t is None else tui_canvas.fmt_tick(t), "?" if tf is None else tui_canvas.fmt_tick(tf)),
            "elapsed %s" % format_duration(info.get("elapsed")),
        ]
        if info.get("solver_status"):
            parts.append("[%s %sit %satt]" % (info.get("solver_status"), info.get("iters"), info.get("attempts")))
        line1 = "  ".join(parts)

        frac = info.get("progress")
        bar_w = max(10, min(40, width // 3))
        if frac is None:
            bar = "[" + "?" * bar_w + "]    "
        else:
            filled = int(round(frac * bar_w))
            bar = "[" + self._c(32, "#" * filled) + "-" * (bar_w - filled) + "] %3d%%" % int(frac * 100)
        line2 = "%s  %s  %s (%s)  host %s pid %s" % (
            bar, info.get("engngmodel") or "", info.get("oofem") or "", info.get("git") or "",
            info.get("host") or "-", info.get("pid") or "-")
        note = info.get("warning") or self.msg
        line3 = (self._c(33, note) + "   " if note else "") + self._c(2, HELP)
        if info.get("status") == "no file":
            line3 = self._c(33, "waiting for %s" % info.get("file")) + "   " + self._c(2, HELP)
        return [tui_canvas.pad(line1, width), tui_canvas.pad(line2, width), tui_canvas.pad(line3, width)]

    def chart_lines(self, chart_id, width, height, index):
        view = self.views.get(chart_id)
        if view is None or view.spec is None:
            return [" " * width for _ in range(height)]
        spec = view.spec
        title = "[%d/%d] %s" % (index + 1, len(self.order), view.title)
        xlabel = "iteration" if spec.kind == "convergence" else spec.x
        try:
            lines = self._render(view.curves, width, height, title, xlabel, spec.logx, spec.logy, self.color)
        except Exception as e:  # never let a rendering problem kill the viewer
            lines = [tui_canvas.pad(title, width), "render error: %s" % e]
        lines = [tui_canvas.pad(line, width) for line in lines[:height]]
        return lines + [" " * width] * (height - len(lines))

    def draw(self):
        size = shutil.get_terminal_size()
        width, height = size.columns, size.lines
        lines = self.header_lines(width)
        avail = height - len(lines) - 1
        if not self.order:
            lines.append("")
            lines.append("No charts. Press 'a' to add a chart or 'c' for the convergence chart.")
        elif self.grid and len(self.order) > 1:
            ncols = 2 if width >= 100 else 1
            nrows = math.ceil(len(self.order) / ncols)
            ch = max(6, avail // nrows)
            cw = width // ncols
            for r in range(nrows):
                blocks = []
                for c in range(ncols):
                    i = r * ncols + c
                    if i < len(self.order):
                        blocks.append(self.chart_lines(self.order[i], cw - 1, ch, i))
                    else:
                        blocks.append([" " * (cw - 1)] * ch)
                for k in range(ch):
                    lines.append(" ".join(b[k] for b in blocks))
        else:
            self.current = min(self.current, len(self.order) - 1)
            lines += self.chart_lines(self.order[self.current], width, max(6, avail), self.current)
        lines = lines[:height - 1]
        frame = "\x1b[H" + "\n".join(line + "\x1b[0m\x1b[K" for line in lines) + "\x1b[J"
        sys.stdout.write(frame)
        sys.stdout.flush()
        self.dirty = False
