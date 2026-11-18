"""Optional chart renderer of the terminal frontend using plotext (``--tui-renderer plotext``).

Has the same signature as the built-in renderer, so the terminal frontend (layout, keys,
info header) is shared; only the drawing of the chart differs.

Both plotext generations are supported, selected by the API found at import:

* plotext 6 - object API (``plotext.figure`` with ``signal``/``draw``/``build``); draws with a
  compiled C++ kernel, shipped as prebuilt wheels for common platforms,
* plotext 5 - module API (``clf``/``plotsize``/``plot``/``build``); pure Python, installable
  everywhere (``pip install "plotext<6"``).

Importing this module raises ImportError if plotext is missing, its kernel cannot be loaded
or its API is unknown; the terminal frontend then falls back to the built-in braille renderer.
"""

import plotext

from . import tui_canvas

_figure = getattr(plotext, "figure", None)
if all(hasattr(_figure, n) for n in ("signal", "draw", "build", "ruler", "plot_size", "clear")):
    PLOTEXT_API = 6
elif all(hasattr(plotext, n) for n in ("clf", "plotsize", "plot", "build", "xscale", "yscale", "theme")):
    PLOTEXT_API = 5
else:
    raise ImportError("unsupported plotext version (plotext 5 or 6 is supported)")

#: colours of successive curves (plotext 5; plotext 6 cycles its theme colours itself)
COLORS = ("blue", "red", "green", "magenta", "orange", "cyan", "gray", "violet", "white", "yellow")


def _is_constant(values):
    return len(values) > 0 and all(v == values[0] for v in values)


def _render_v6(curves, width, height, title, xlabel, logx, logy, use_color):
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
            fig.draw(fig.signal(xs, [c.y[0]] * n, marker="·").label(c.label))
        else:
            fig.draw(fig.signal(c.x, c.y).lines().label(c.label))
    return fig.build().string(colorless=not use_color)


def _render_v5(curves, width, height, title, xlabel, logx, logy, use_color):
    plotext.clf()
    plotext.plotsize(width, height)
    plotext.theme("pro" if use_color else "clear")
    plotext.title(title)
    plotext.xlabel(xlabel)
    plotext.xscale("log" if logx else "linear")
    plotext.yscale("log" if logy else "linear")
    for i, c in enumerate(curves):
        if not c.x:
            continue
        # plotext 5 has no dashed lines; dashed curves (tolerances) are drawn by dots
        marker = "dot" if c.style == "dash" else "braille"
        plotext.plot(c.x, c.y, label=c.label, marker=marker, color=COLORS[i % len(COLORS)])
    return plotext.build()


def render_chart(curves, width, height, title, xlabel, logx, logy, use_color):
    xs = [v for c in curves for v in c.x]
    ys = [v for c in curves for v in c.y]
    # without data, or with a zero data range on a log axis (all values equal, e.g. only the
    # tolerance line of a converged step), plotext fails; the built-in renderer handles these cases
    if not xs or (logx and min(xs) == max(xs)) or (logy and min(ys) == max(ys)):
        return tui_canvas.render_chart(curves, width, height, title, xlabel, logx, logy, "braille", use_color)
    render = _render_v6 if PLOTEXT_API == 6 else _render_v5
    lines = render(curves, width, height, title, xlabel, logx, logy, use_color).split("\n")
    lines = [tui_canvas.pad(line, width) for line in lines[:height]]
    return lines + [" " * width] * (height - len(lines))
