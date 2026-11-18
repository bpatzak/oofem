"""Toolkit independent viewer logic: polls the progress file and drives a frontend."""

from .protocol import ProgressReader, RunState
from .charts import Layout, ChartSpec, chart_curves, convergence_title


class ViewerController:
    """Owns the reader, run state and chart layout; pushes updates to a frontend.

    :param path: progress file to follow
    :param frontend: :class:`~oofemprogress.frontend.Frontend` instance
    :param layout: initial :class:`Layout`; if None, the default layout from the header is used
    :param poll_ms: polling period in milliseconds
    """

    def __init__(self, path, frontend, layout=None, poll_ms=500):
        self.path = path
        self.frontend = frontend
        self.reader = ProgressReader(path)
        self.state = RunState()
        self.user_layout = layout is not None
        self.layout = layout or Layout()
        self.poll_ms = poll_ms
        self.views = {}          # chart id -> ChartView
        self._drawn_revision = -1

    # ------------------------------------------------------------------ lifecycle
    def start(self):
        """Builds the frontend, starts polling and runs the frontend's event loop."""
        self.frontend.build(self)
        self._rebuild_views()
        self.poll()
        self.frontend.run()

    def poll(self):
        """Reads new records and refreshes the frontend; reschedules itself."""
        try:
            self.update()
        finally:
            self.frontend.schedule(self.poll_ms, self.poll)

    def update(self):
        records = self.reader.poll()
        header_changed = False
        for rec in records:
            if rec.get("type") in ("header", "_reset"):
                header_changed = True
            self.state.apply(rec)
        if header_changed and not self.user_layout and self.state.header is not None:
            # new run: charts follow the header unless the user supplied a layout
            self.layout = Layout.default_for(self.state)
            self._rebuild_views()
        info = self.state.info()
        info["file"] = self.path
        if not self.reader.exists():
            info["status"] = "no file"
        self.frontend.set_info(info)
        if self.state.revision != self._drawn_revision:
            self.redraw()

    def redraw(self):
        for spec in self.layout.charts:
            view = self.views.get(spec.id)
            if view is not None:
                self.draw_chart(spec, view)
        self._drawn_revision = self.state.revision

    def draw_chart(self, spec, view):
        title = convergence_title(self.state) if spec.kind == "convergence" and not spec.title else spec.label()
        view.update(spec, chart_curves(spec, self.state), title)

    def _rebuild_views(self):
        for chart_id in list(self.views):
            self.frontend.remove_chart_view(chart_id)
        self.views = {}
        for spec in self.layout.charts:
            self.views[spec.id] = self.frontend.add_chart_view(spec)
        self._drawn_revision = -1

    # ------------------------------------------------------------------ user actions
    def series_names(self):
        """Names selectable as chart axes."""
        return self.state.series_names()

    def add_chart(self, spec):
        """Adds a chart (called by frontends after the user defined it)."""
        unknown = [n for n in [spec.x] + list(spec.y)
                   if spec.kind == "xy" and n not in self.series_names()]
        if unknown:
            raise ValueError("unknown series: %s" % ", ".join(unknown))
        self.layout.add(spec)
        self.user_layout = True
        view = self.frontend.add_chart_view(spec)
        self.views[spec.id] = view
        self.draw_chart(spec, view)
        return spec

    def add_convergence_chart(self):
        return self.add_chart(ChartSpec(kind="convergence", logy=True))

    def remove_chart(self, chart_id):
        self.layout.remove(chart_id)
        self.user_layout = True
        if chart_id in self.views:
            self.frontend.remove_chart_view(chart_id)
            del self.views[chart_id]

    def save_layout(self, path):
        self.layout.save(path)
        self.frontend.message("layout saved to %s" % path)

    def load_layout(self, path):
        self.layout = Layout.load(path)
        self.user_layout = True
        self._rebuild_views()
        self.redraw()
        self.frontend.message("layout loaded from %s" % path)
