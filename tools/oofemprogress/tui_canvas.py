"""Text-mode chart rendering using the standard library only.

Two canvases are provided:

* :class:`BrailleCanvas` uses the Unicode braille block (U+2800..U+28FF): every character cell
  holds 2x4 dots, giving a reasonably smooth plot in any terminal font with braille glyphs,
* :class:`AsciiCanvas` uses one marker character per cell; it works with any font and encoding
  (e.g. the legacy Windows console).

:func:`render_chart` draws curves with axes, tick labels and a legend into a list of lines.
"""

import math

#: ANSI foreground colours used for successive curves.
ANSI_COLORS = (34, 31, 32, 35, 33, 36, 94, 91, 92, 95)
RESET = "\x1b[0m"
DIM = "\x1b[2m"
#: Markers of successive curves on the ASCII canvas.
ASCII_MARKERS = "*o+x#@%&=$"


def ansi(code):
    return "\x1b[%dm" % code


class _Canvas:
    """Common part: pixel grid mapped onto a grid of character cells."""

    #: pixels per character cell
    px = 1
    py = 1

    def __init__(self, cols, rows):
        self.cols = max(1, cols)
        self.rows = max(1, rows)
        self.width = self.cols * self.px
        self.height = self.rows * self.py
        self.colors = [[None] * self.cols for _ in range(self.rows)]

    def line(self, x0, y0, x1, y1, curve, dash=False):
        """Bresenham line between pixel coordinates (y grows downwards)."""
        x0, y0, x1, y1 = int(round(x0)), int(round(y0)), int(round(x1)), int(round(y1))
        dx, dy = abs(x1 - x0), -abs(y1 - y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx + dy
        n = 0
        # guard against absurd lengths (e.g. huge values before clipping)
        for _ in range(4 * (self.width + self.height) + 4):
            if not dash or (n // 2) % 2 == 0:
                self.set(x0, y0, curve)
            n += 1
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx
            if e2 <= dx:
                err += dx
                y0 += sy

    def set(self, x, y, curve):
        raise NotImplementedError

    def render_rows(self, use_color):
        raise NotImplementedError


class BrailleCanvas(_Canvas):
    px = 2
    py = 4
    # bit of the dot at [row within cell][column within cell]
    _DOTS = ((0x01, 0x08), (0x02, 0x10), (0x04, 0x20), (0x40, 0x80))

    def __init__(self, cols, rows):
        super().__init__(cols, rows)
        self.bits = [[0] * self.cols for _ in range(self.rows)]

    def set(self, x, y, curve):
        if 0 <= x < self.width and 0 <= y < self.height:
            cx, cy = x // 2, y // 4
            self.bits[cy][cx] |= self._DOTS[y % 4][x % 2]
            self.colors[cy][cx] = curve

    def render_rows(self, use_color):
        out = []
        for bits_row, color_row in zip(self.bits, self.colors):
            out.append(_colorize([chr(0x2800 + b) if b else " " for b in bits_row], color_row, use_color))
        return out


class AsciiCanvas(_Canvas):
    def __init__(self, cols, rows):
        super().__init__(cols, rows)
        self.chars = [[" "] * self.cols for _ in range(self.rows)]

    def set(self, x, y, curve):
        if 0 <= x < self.width and 0 <= y < self.height:
            self.chars[y][x] = ASCII_MARKERS[curve % len(ASCII_MARKERS)]
            self.colors[y][x] = curve

    def render_rows(self, use_color):
        return [_colorize(chars, colors, use_color) for chars, colors in zip(self.chars, self.colors)]


def _colorize(chars, colors, use_color):
    if not use_color:
        return "".join(chars)
    out = []
    current = None
    for ch, c in zip(chars, colors):
        if ch == " ":
            c = None
        if c != current:
            out.append(RESET if c is None else ansi(ANSI_COLORS[c % len(ANSI_COLORS)]))
            current = c
        out.append(ch)
    if current is not None:
        out.append(RESET)
    return "".join(out)


def fmt_tick(v):
    if v == 0:
        return "0"
    return "%.3g" % v


def _snap(v, lo, hi, log):
    """Shows round-off noise around zero (e.g. -8.9e-33 on a range of 100) as 0."""
    if not log and abs(v) < 1e-9 * (hi - lo):
        return 0.0
    return v


def _range(values, log):
    vals = [math.log10(v) if log else v for v in values]
    lo, hi = min(vals), max(vals)
    if hi - lo < 1e-300:
        pad = abs(lo) * 0.05 or 0.5
        lo, hi = lo - pad, hi + pad
    return lo, hi


def visible_len(text):
    """Length of text without ANSI escape sequences."""
    n = 0
    i = 0
    while i < len(text):
        if text[i] == "\x1b":
            j = text.find("m", i)
            i = len(text) if j < 0 else j + 1
            continue
        n += 1
        i += 1
    return n


def pad(text, width):
    """Pads (or truncates plain) text to exactly ``width`` visible characters."""
    n = visible_len(text)
    if n <= width:
        return text + " " * (width - n)
    if "\x1b" not in text:
        return text[:width]
    # truncate text with escapes: keep escapes, drop visible chars beyond width
    out, count, i = [], 0, 0
    while i < len(text) and count < width:
        if text[i] == "\x1b":
            j = text.find("m", i)
            j = len(text) - 1 if j < 0 else j
            out.append(text[i:j + 1])
            i = j + 1
            continue
        out.append(text[i])
        count += 1
        i += 1
    return "".join(out) + RESET


def render_chart(curves, width, height, title="", xlabel="", logx=False, logy=False,
                 mode="braille", use_color=True):
    """Renders curves into exactly ``height`` lines of ``width`` visible characters.

    :param curves: list of objects with ``label``, ``x``, ``y`` and ``style`` ("line"/"dash")
    :param mode: "braille" or "ascii"
    """
    width = max(width, 20)
    height = max(height, 6)
    lines = [pad(title, width)]

    # legend
    legend = []
    for i, c in enumerate(curves):
        mark = ASCII_MARKERS[i % len(ASCII_MARKERS)] if mode == "ascii" else "■"
        item = "%s %s" % (mark, c.label)
        if use_color:
            item = ansi(ANSI_COLORS[i % len(ANSI_COLORS)]) + item + RESET
        legend.append(item)
    lines.append(pad("  ".join(legend), width))

    xs = [v for c in curves for v in c.x]
    ys = [v for c in curves for v in c.y]
    plot_rows = height - 4
    if not xs or not ys:
        body = ["" for _ in range(plot_rows)]
        body[plot_rows // 2] = " " * max(0, width // 2 - 5) + "(no data)"
        lines += [pad(b, width) for b in body]
        lines += [pad("", width), pad("", width)]
        return lines

    xlo, xhi = _range(xs, logx)
    ylo, yhi = _range(ys, logy)
    ylabels = [fmt_tick(_snap(10 ** v if logy else v, ylo, yhi, logy)) for v in (yhi, (ylo + yhi) / 2, ylo)]
    label_w = max(len(s) for s in ylabels) + 1
    plot_cols = max(5, width - label_w - 1)

    canvas = AsciiCanvas(plot_cols, plot_rows) if mode == "ascii" else BrailleCanvas(plot_cols, plot_rows)

    def to_px(x, y):
        tx = (math.log10(x) if logx else x)
        ty = (math.log10(y) if logy else y)
        px = (tx - xlo) / (xhi - xlo) * (canvas.width - 1)
        py = (yhi - ty) / (yhi - ylo) * (canvas.height - 1)
        return px, py

    for i, c in enumerate(curves):
        pts = [to_px(x, y) for x, y in zip(c.x, c.y)]
        if len(pts) == 1:
            canvas.set(int(round(pts[0][0])), int(round(pts[0][1])), i)
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            canvas.line(x0, y0, x1, y1, i, dash=(c.style == "dash"))

    rows = canvas.render_rows(use_color)
    mid = plot_rows // 2
    for r, row in enumerate(rows):
        if r == 0:
            label = ylabels[0]
        elif r == mid:
            label = ylabels[1]
        elif r == plot_rows - 1:
            label = ylabels[2]
        else:
            label = ""
        lines.append(pad(label.rjust(label_w) + "|" + row, width))

    # x axis and tick labels (min, middle, max) and the axis name
    lines.append(pad(" " * label_w + "+" + "-" * plot_cols, width))
    xt = [fmt_tick(_snap(10 ** v if logx else v, xlo, xhi, logx)) for v in (xlo, (xlo + xhi) / 2, xhi)]
    axis = [" "] * (plot_cols + 1)

    def place(text, pos):
        pos = max(0, min(pos, len(axis) - len(text)))
        for k, ch in enumerate(text):
            if pos + k < len(axis):
                axis[pos + k] = ch

    place(xt[0], 0)
    place(xt[1], plot_cols // 2 - len(xt[1]) // 2)
    place(xt[2], plot_cols + 1 - len(xt[2]))
    tick_line = " " * label_w + "".join(axis)
    if xlabel:
        name = "[%s]" % xlabel
        # put the axis name between the min and middle tick if it fits
        start = label_w + len(xt[0]) + 2
        end = label_w + plot_cols // 2 - len(xt[1]) // 2 - 1
        if end - start >= len(name):
            tick_line = tick_line[:start] + name + tick_line[start + len(name):]
    lines.append(pad(tick_line, width))
    return lines
