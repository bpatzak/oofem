"""Toolkit independent chart descriptions and data extraction."""

import json
import math
from dataclasses import dataclass, field, asdict

#: Chart kinds: ``xy`` plots series against a series, ``convergence`` shows the error norms
#: of the solver iterations of the most recent step attempt.
KINDS = ("xy", "convergence")


@dataclass
class ChartSpec:
    x: str = "time"
    y: list = field(default_factory=list)
    title: str = ""
    kind: str = "xy"
    logx: bool = False
    logy: bool = False
    id: int = 0

    def label(self):
        if self.title:
            return self.title
        if self.kind == "convergence":
            return "Solver convergence"
        return "%s vs %s" % (", ".join(self.y), self.x)

    def to_dict(self):
        d = asdict(self)
        d.pop("id")
        return d

    @classmethod
    def from_dict(cls, d):
        known = {k: d[k] for k in ("x", "y", "title", "kind", "logx", "logy") if k in d}
        spec = cls(**known)
        if isinstance(spec.y, str):
            spec.y = [spec.y]
        return spec


@dataclass
class Curve:
    label: str
    x: list
    y: list
    style: str = "line"   # "line" or "dash" (e.g. tolerances)


class Layout:
    """Ordered list of charts with unique ids; can be saved to and loaded from JSON."""

    def __init__(self, charts=None):
        self.charts = []
        self._next_id = 1
        for spec in charts or []:
            self.add(spec)

    def add(self, spec):
        spec.id = self._next_id
        self._next_id += 1
        self.charts.append(spec)
        return spec

    def remove(self, chart_id):
        self.charts = [c for c in self.charts if c.id != chart_id]

    def get(self, chart_id):
        for c in self.charts:
            if c.id == chart_id:
                return c
        return None

    def save(self, path):
        with open(path, "w", encoding="utf-8") as f:
            json.dump({"charts": [c.to_dict() for c in self.charts]}, f, indent=2)

    @classmethod
    def load(cls, path):
        with open(path, encoding="utf-8") as f:
            data = json.load(f)
        return cls([ChartSpec.from_dict(d) for d in data.get("charts", [])])

    @classmethod
    def default_for(cls, state):
        """Charts suggested by the header (``plots``), or the first channel against time,
        followed by the convergence chart if iteration records are written."""
        header = state.header or {}
        names = set(state.series_names())
        charts = []
        for p in header.get("plots") or []:
            if p.get("x") in names and p.get("y") in names:
                charts.append(ChartSpec(x=p["x"], y=[p["y"]]))
        if not charts and state.channels:
            charts.append(ChartSpec(x="time", y=[state.channels[0]["id"]]))
        if header.get("iterations"):
            charts.append(ChartSpec(kind="convergence", logy=True))
        return cls(charts)


def parse_chart_option(text):
    """Parses the ``--chart`` command line option ``X:Y[,Y2...]``; ``conv`` means the convergence chart."""
    if text.strip().lower() in ("conv", "convergence"):
        return ChartSpec(kind="convergence", logy=True)
    if ":" not in text:
        raise ValueError("chart must be given as X:Y[,Y2...], got %r" % text)
    x, ys = text.split(":", 1)
    return ChartSpec(x=x.strip(), y=[y.strip() for y in ys.split(",") if y.strip()])


def _usable(v, log):
    return v is not None and isinstance(v, (int, float)) and math.isfinite(v) and (not log or v > 0)


def chart_curves(spec, state):
    """Returns the list of :class:`Curve` to draw for the chart in the current state."""
    if spec.kind == "convergence":
        return convergence_curves(state)
    xs = state.series.get(spec.x, [])
    curves = []
    for name in spec.y:
        ys = state.series.get(name, [])
        cx, cy = [], []
        for a, b in zip(xs, ys):
            if _usable(a, spec.logx) and _usable(b, spec.logy):
                cx.append(a)
                cy.append(b)
        curves.append(Curve(name, cx, cy))
    return curves


def convergence_curves(state):
    """Force and displacement error norms per DOF group vs. iteration number for the most
    recent step attempt, with the relative tolerances as dashed lines."""
    key = state.last_iter_key
    if key is None:
        return []
    iters = state.iterations.get(key, [])
    if not iters:
        return []
    groups = iters[-1].get("groups") or []
    curves = []
    for what, prefix in (("force_err", "F"), ("disp_err", "D")):
        for gi, group in enumerate(groups):
            cx, cy = [], []
            for rec in iters:
                values = rec.get(what) or []
                if gi < len(values) and _usable(values[gi], True):
                    cx.append(rec.get("iter"))
                    cy.append(values[gi])
            if cy:
                curves.append(Curve("%s %s" % (prefix, group), cx, cy))
    last_iter = max(1, iters[-1].get("iter", 1))
    for tol, label in (("rtolf", "rtolf"), ("rtold", "rtold")):
        value = iters[-1].get(tol)
        if _usable(value, True):
            curves.append(Curve(label, [0, last_iter], [value, value], style="dash"))
    return curves


def convergence_title(state):
    key = state.last_iter_key
    if key is None:
        return "Solver convergence"
    step, attempt = key
    title = "Solver convergence: step %s" % step
    if attempt and attempt > 1:
        title += " (attempt %s)" % attempt
    return title
