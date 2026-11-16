"""Optional chart renderer of the terminal frontend using plotext (``--tui-renderer plotext``).

Has the same signature as the built-in renderer, so the terminal frontend (layout, keys,
info header) is shared; only the drawing of the chart differs.

Uses the object API of plotext 6 (``plotext.figure`` with ``signal``/``draw``/``build``).
Importing this module raises ImportError for older plotext versions (or when plotext's
compiled kernel is not available); the terminal frontend then falls back to the built-in
braille renderer.
"""

import plotext

from . import tui_canvas

_figure = getattr(plotext, "figure", None)
if not all(hasattr(_figure, name) for name in ("signal", "draw", "build", "ruler", "plot_size", "line", "clear")):
    raise ImportError("plotext>=6 is required")


def _is_constant(values):
    return len(values) > 0 and all(v == values[0] for v in values)


def render_chart(curves, width, height, title, xlabel, logx, logy, use_color):
    xs = [v for c in curves for v in c.x]
    ys = [v for c in curves for v in c.y]
    # plotext 6 fails on log axes without data or with a zero data range (all values equal, e.g. only
    # the tolerance line of a converged step); the built-in renderer handles these cases
    if not xs or (logx and min(xs) == max(xs)) or (logy and min(ys) == max(ys)):
        return tui_canvas.render_chart(curves, width, height, title, xlabel, logx, logy, "braille", use_color)
    fig = plotext.figure
    fig.clear()  # the master figure is shared: reset data, settings and size from the previous chart
    fig.plot_size(width, height)
    fig.title(title)
    fig.label(xlabel, axis="x")
    if logx:
        fig.ruler("x").scale("log")
    if logy:
        fig.ruler("y").scale("log")
    for c in curves:
        if not c.x:
            continue
        if c.style == "dash" and _is_constant(c.y) and len(c.x) >= 2:
            # constant reference values (solver tolerances) as a row of unconnected dots; plotext's
            # own line() is not placed correctly on logarithmic axes
            n = max(2, width // 3)
            x0, x1 = min(c.x), max(c.x)
            xs = [x0 + (x1 - x0) * k / (n - 1) for k in range(n)]
            fig.draw(fig.signal(xs, [c.y[0]] * n, marker="\u00b7").label(c.label))
        else:
            fig.draw(fig.signal(c.x, c.y).lines().label(c.label))
    lines = fig.build().string(colorless=not use_color).split("\n")
    lines = [tui_canvas.pad(line, width) for line in lines[:height]]
    return lines + [" " * width] * (height - len(lines))
