"""Optional chart renderer of the terminal frontend using plotext (``--tui-renderer plotext``).

Has the same signature as the built-in renderer, so the terminal frontend (layout, keys,
info header) is shared; only the drawing of the chart differs.

Uses the plotext 5.x API (``clf``, ``plotsize``, ``plot``, ``build``). plotext 6 is a rewrite
with a different API; importing this module raises ImportError in that case and the
terminal frontend falls back to the built-in renderer.
"""

import plotext as plt

from . import tui_canvas

if not all(hasattr(plt, name) for name in ("clf", "plotsize", "plot", "build", "xscale", "yscale", "theme")):
    raise ImportError("unsupported plotext version (plotext<6 is required)")

COLORS = ("blue", "red", "green", "magenta", "orange", "cyan", "gray", "violet", "white", "yellow")


def render_chart(curves, width, height, title, xlabel, logx, logy, use_color):
    if not any(c.x for c in curves):
        # plotext cannot draw an empty plot with log axes; the built-in renderer handles it
        return tui_canvas.render_chart(curves, width, height, title, xlabel, logx, logy, "braille", use_color)
    plt.clf()
    plt.plotsize(width, height)
    plt.theme("pro" if use_color else "clear")
    plt.title(title)
    plt.xlabel(xlabel)
    plt.xscale("log" if logx else "linear")
    plt.yscale("log" if logy else "linear")
    for i, c in enumerate(curves):
        if not c.x:
            continue
        # plotext has no dashed lines; dashed curves (tolerances) are drawn by dots
        marker = "dot" if c.style == "dash" else "braille"
        plt.plot(c.x, c.y, label=c.label, marker=marker, color=COLORS[i % len(COLORS)])
    lines = plt.build().split("\n")
    lines = [tui_canvas.pad(line, width) for line in lines[:height]]
    return lines + [" " * width] * (height - len(lines))
