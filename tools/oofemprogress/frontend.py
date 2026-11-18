"""Interface implemented by all viewer frontends (Qt, Tk, terminal).

The :class:`~oofemprogress.controller.ViewerController` owns the data (reader, run state,
chart layout) and calls these methods; a frontend only creates widgets and draws.
"""


class ChartView:
    """A single chart displayed by a frontend."""

    def update(self, spec, curves, title):
        """Redraws the chart.

        :param spec: :class:`~oofemprogress.charts.ChartSpec` of the chart (axes names, log scales)
        :param curves: list of :class:`~oofemprogress.charts.Curve`
        :param title: title to display
        """
        raise NotImplementedError


class Frontend:
    """Base class of viewer frontends."""

    #: Short name used by the ``--backend`` option.
    name = "base"

    def build(self, controller):
        """Creates the main window/screen; called once before :meth:`run`."""
        self.controller = controller

    def set_info(self, info):
        """Updates the job information panel (dict produced by ``RunState.info()``)."""
        raise NotImplementedError

    def add_chart_view(self, spec):
        """Creates a view for the chart and returns its :class:`ChartView`."""
        raise NotImplementedError

    def remove_chart_view(self, chart_id):
        """Removes the view of the chart with given id."""
        raise NotImplementedError

    def schedule(self, ms, callback):
        """Calls ``callback`` once after ``ms`` milliseconds from the frontend's event loop."""
        raise NotImplementedError

    def run(self):
        """Runs the event loop until the user quits."""
        raise NotImplementedError

    def message(self, text):
        """Shows a short status message (e.g. 'layout saved')."""
        pass


def available_backends():
    """Names of backends whose dependencies are importable, in order of preference."""
    names = []
    try:
        import PySide6  # noqa: F401
        import pyqtgraph  # noqa: F401
        names.append("qt")
    except ImportError:
        pass
    try:
        import tkinter  # noqa: F401
        import matplotlib  # noqa: F401
        names.append("tk")
    except ImportError:
        pass
    names.append("tui")
    return names


def create_frontend(name, **options):
    """Creates the frontend of given name ('auto' picks the first available)."""
    if name == "auto":
        name = available_backends()[0]
    if name == "qt":
        from .fe_qt import QtFrontend
        return QtFrontend(**options)
    if name == "tk":
        from .fe_tk import TkFrontend
        return TkFrontend(**options)
    if name == "tui":
        from .fe_tui import TuiFrontend
        return TuiFrontend(**options)
    raise ValueError("unknown backend %r" % name)
