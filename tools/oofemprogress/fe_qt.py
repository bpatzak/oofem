"""Qt frontend: PySide6 + pyqtgraph.

Fast live plotting with zoom/pan (mouse wheel, drag; the 'A' button in a chart corner restores
automatic range), cursor coordinate readout, light/dark theme, add/remove charts and
saving/loading of the chart layout.
"""

import sys

from PySide6 import QtCore, QtGui, QtWidgets
import pyqtgraph as pg

from .frontend import Frontend, ChartView
from .charts import ChartSpec
from .protocol import format_duration

PALETTE = ("#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e",
           "#17becf", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22")
STATUS_COLORS = {"running": "#2ca02c", "finished": "#1f77b4", "crashed": "#d62728"}
THEMES = {
    False: {"bg": "#ffffff", "fg": "#303030", "grid": 0.25},
    True: {"bg": "#1e1f22", "fg": "#d0d0d0", "grid": 0.2},
}


def _system_is_dark(app):
    hints = app.styleHints()
    if hasattr(hints, "colorScheme"):  # Qt >= 6.5
        return hints.colorScheme() == QtCore.Qt.ColorScheme.Dark
    return app.palette().color(QtGui.QPalette.Window).lightness() < 128


def _dark_palette():
    p = QtGui.QPalette()
    roles = {
        QtGui.QPalette.Window: "#2b2d30", QtGui.QPalette.WindowText: "#dfe1e5",
        QtGui.QPalette.Base: "#1e1f22", QtGui.QPalette.AlternateBase: "#2b2d30",
        QtGui.QPalette.Text: "#dfe1e5", QtGui.QPalette.Button: "#3c3f41",
        QtGui.QPalette.ButtonText: "#dfe1e5", QtGui.QPalette.Highlight: "#3574f0",
        QtGui.QPalette.HighlightedText: "#ffffff", QtGui.QPalette.ToolTipBase: "#2b2d30",
        QtGui.QPalette.ToolTipText: "#dfe1e5", QtGui.QPalette.PlaceholderText: "#8c8c8c",
    }
    for role, color in roles.items():
        p.setColor(role, QtGui.QColor(color))
    return p


class QtChartView(ChartView):
    def __init__(self, frontend, spec):
        self.frontend = frontend
        self.spec = spec
        self.items = {}

        self.box = QtWidgets.QFrame()
        self.box.setFrameShape(QtWidgets.QFrame.StyledPanel)
        layout = QtWidgets.QVBoxLayout(self.box)
        layout.setContentsMargins(6, 4, 6, 6)
        head = QtWidgets.QHBoxLayout()
        self.title = QtWidgets.QLabel()
        font = self.title.font()
        font.setBold(True)
        self.title.setFont(font)
        self.coords = QtWidgets.QLabel()
        self.coords.setStyleSheet("color: gray")
        close = QtWidgets.QToolButton()
        close.setText("✕")
        close.setToolTip("Remove chart")
        close.setAutoRaise(True)
        close.clicked.connect(lambda: frontend.controller.remove_chart(spec.id))
        head.addWidget(self.title, 1)
        head.addWidget(self.coords)
        head.addWidget(close)
        layout.addLayout(head)

        self.plot = pg.PlotWidget()
        self.plot.setMinimumHeight(260)
        self.legend = self.plot.addLegend(offset=(-10, 10))
        self.plot.setLogMode(x=spec.logx, y=spec.logy)
        self.plot.setLabel("bottom", "iteration" if spec.kind == "convergence" else spec.x)
        if spec.kind == "xy" and len(spec.y) == 1:
            self.plot.setLabel("left", spec.y[0])
        layout.addWidget(self.plot, 1)

        self.vline = pg.InfiniteLine(angle=90, movable=False, pen=pg.mkPen("#888888", style=QtCore.Qt.DotLine))
        self.hline = pg.InfiniteLine(angle=0, movable=False, pen=pg.mkPen("#888888", style=QtCore.Qt.DotLine))
        for line in (self.vline, self.hline):
            line.setVisible(False)
            self.plot.addItem(line, ignoreBounds=True)
        self.proxy = pg.SignalProxy(self.plot.scene().sigMouseMoved, rateLimit=30, slot=self._mouse_moved)
        self.apply_theme(frontend.dark)

    def apply_theme(self, dark):
        theme = THEMES[dark]
        self.plot.setBackground(theme["bg"])
        for name in ("left", "bottom"):
            axis = self.plot.getAxis(name)
            axis.setPen(theme["fg"])
            axis.setTextPen(theme["fg"])
        self.plot.showGrid(x=True, y=True, alpha=theme["grid"])
        self.legend.setLabelTextColor(theme["fg"])
        for _sample, label in self.legend.items:  # existing entries keep their color otherwise
            label.setText(label.text, color=theme["fg"])

    def _mouse_moved(self, event):
        pos = event[0]
        inside = self.plot.sceneBoundingRect().contains(pos)
        self.vline.setVisible(inside)
        self.hline.setVisible(inside)
        if not inside:
            self.coords.setText("")
            return
        p = self.plot.getPlotItem().vb.mapSceneToView(pos)
        self.vline.setPos(p.x())
        self.hline.setPos(p.y())
        x = 10 ** p.x() if self.spec.logx else p.x()
        y = 10 ** p.y() if self.spec.logy else p.y()
        self.coords.setText("x=%.5g  y=%.5g" % (x, y))

    def update(self, spec, curves, title):
        self.title.setText(title)
        seen = set()
        for i, c in enumerate(curves):
            color = PALETTE[i % len(PALETTE)]
            dash = c.style == "dash"
            pen = pg.mkPen(color, width=1.5 if dash else 2,
                           style=QtCore.Qt.DashLine if dash else QtCore.Qt.SolidLine)
            item = self.items.get(c.label)
            if item is None:
                item = self.plot.plot(name=c.label, pen=pen)
                self.items[c.label] = item
            else:
                item.setPen(pen)
            # a lone point would be invisible as a line
            if len(c.x) == 1 and not dash:
                item.setSymbol("o")
                item.setSymbolBrush(color)
                item.setSymbolPen(color)
                item.setSymbolSize(6)
            else:
                item.setSymbol(None)
            item.setData(c.x, c.y)
            seen.add(c.label)
        for label in [k for k in self.items if k not in seen]:
            item = self.items.pop(label)
            self.legend.removeItem(item)
            self.plot.removeItem(item)


class AddChartDialog(QtWidgets.QDialog):
    def __init__(self, parent, names):
        super().__init__(parent)
        self.setWindowTitle("Add chart")
        form = QtWidgets.QFormLayout(self)
        self.title = QtWidgets.QLineEdit()
        self.title.setPlaceholderText("optional")
        self.x = QtWidgets.QComboBox()
        self.x.addItems(names)
        self.y = QtWidgets.QListWidget()
        self.y.addItems(names)
        self.y.setSelectionMode(QtWidgets.QAbstractItemView.ExtendedSelection)
        self.y.setMinimumHeight(180)
        self.logx = QtWidgets.QCheckBox("logarithmic x")
        self.logy = QtWidgets.QCheckBox("logarithmic y")
        buttons = QtWidgets.QDialogButtonBox(QtWidgets.QDialogButtonBox.Ok | QtWidgets.QDialogButtonBox.Cancel)
        buttons.accepted.connect(self.accept)
        buttons.rejected.connect(self.reject)
        self.ok = buttons.button(QtWidgets.QDialogButtonBox.Ok)
        self.ok.setEnabled(False)
        self.y.itemSelectionChanged.connect(lambda: self.ok.setEnabled(bool(self.y.selectedItems())))
        form.addRow("Title", self.title)
        form.addRow("X axis", self.x)
        form.addRow("Y series", self.y)
        form.addRow(self.logx)
        form.addRow(self.logy)
        form.addRow(buttons)

    def spec(self):
        return ChartSpec(x=self.x.currentText(), y=[i.text() for i in self.y.selectedItems()],
                         title=self.title.text().strip(), logx=self.logx.isChecked(), logy=self.logy.isChecked())


class QtFrontend(Frontend):
    name = "qt"

    def __init__(self, theme="auto", **_options):
        self.theme = theme
        self.views = {}
        self.order = []
        self.columns = 2

    # ------------------------------------------------------------------ building
    def build(self, controller):
        super().build(controller)
        self.app = QtWidgets.QApplication.instance() or QtWidgets.QApplication(sys.argv[:1])
        self.app.setStyle("Fusion")
        pg.setConfigOptions(antialias=True)
        self.dark = _system_is_dark(self.app) if self.theme == "auto" else self.theme == "dark"
        self.light_palette = self.app.palette()

        self.win = QtWidgets.QMainWindow()
        self.win.setWindowTitle("OOFEM progress - %s" % controller.path)
        self._build_toolbar()

        central = QtWidgets.QWidget()
        vbox = QtWidgets.QVBoxLayout(central)
        vbox.addWidget(self._build_info())
        self.area = QtWidgets.QScrollArea()
        self.area.setWidgetResizable(True)
        self.charts_widget = QtWidgets.QWidget()
        self.grid = QtWidgets.QGridLayout(self.charts_widget)
        self.grid.setContentsMargins(0, 0, 0, 0)
        self.area.setWidget(self.charts_widget)
        vbox.addWidget(self.area, 1)
        self.win.setCentralWidget(central)
        self.win.resize(1280, 860)
        self.apply_theme(self.dark)

    def _build_toolbar(self):
        tb = self.win.addToolBar("main")
        tb.setMovable(False)
        tb.addAction("Add chart…", self.add_chart_dialog)
        tb.addAction("Convergence chart", self.controller.add_convergence_chart)
        tb.addSeparator()
        tb.addAction("Save layout…", self.save_layout_dialog)
        tb.addAction("Load layout…", self.load_layout_dialog)
        tb.addSeparator()
        tb.addWidget(QtWidgets.QLabel(" Columns "))
        cols = QtWidgets.QSpinBox()
        cols.setRange(1, 4)
        cols.setValue(self.columns)
        cols.valueChanged.connect(self._set_columns)
        tb.addWidget(cols)
        tb.addSeparator()
        self.theme_action = tb.addAction("Dark theme")
        self.theme_action.setCheckable(True)
        self.theme_action.toggled.connect(self.apply_theme)

    def _build_info(self):
        box = QtWidgets.QGroupBox()
        grid = QtWidgets.QGridLayout(box)
        self.fields = {}
        items = [("job", "Job"), ("status", "Status"), ("step", "Step"), ("time", "Time"),
                 ("elapsed", "Elapsed"), ("solver", "Last step"), ("engngmodel", "Problem"),
                 ("oofem", "OOFEM"), ("host", "Host / PID"), ("start", "Started")]
        for i, (key, label) in enumerate(items):
            row, col = divmod(i, 4)
            name = QtWidgets.QLabel(label + ":")
            name.setStyleSheet("color: gray")
            value = QtWidgets.QLabel("-")
            value.setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
            grid.addWidget(name, row, 2 * col)
            grid.addWidget(value, row, 2 * col + 1)
            self.fields[key] = value
        grid.setColumnStretch(7, 1)
        # the description may be long, it gets a row of its own
        name = QtWidgets.QLabel("Description:")
        name.setStyleSheet("color: gray")
        self.fields["description"] = QtWidgets.QLabel("-")
        self.fields["description"].setTextInteractionFlags(QtCore.Qt.TextSelectableByMouse)
        grid.addWidget(name, 3, 0)
        grid.addWidget(self.fields["description"], 3, 1, 1, 7)
        self.progress = QtWidgets.QProgressBar()
        self.progress.setRange(0, 1000)
        self.progress.setTextVisible(True)
        grid.addWidget(self.progress, 4, 0, 1, 8)
        self.fields["status"].setStyleSheet("font-weight: bold")
        return box

    # ------------------------------------------------------------------ Frontend interface
    def set_info(self, info):
        def show(key, text):
            self.fields[key].setText("-" if text is None or text == "" else str(text))
        show("job", info.get("job"))
        status = info.get("status") or "-"
        color = STATUS_COLORS.get(status, "#d08000")
        self.fields["status"].setText(status.upper())
        self.fields["status"].setStyleSheet("font-weight: bold; color: %s" % color)
        show("step", "%s / %s" % (info.get("step") if info.get("step") is not None else "-", info.get("nsteps") or "?"))
        t, tf = info.get("time"), info.get("final_time")
        show("time", "%s / %s" % ("-" if t is None else "%.6g" % t, "?" if tf is None else "%.6g" % tf))
        show("elapsed", format_duration(info.get("elapsed")))
        if info.get("solver_status"):
            show("solver", "%s, %s iterations, %s attempt(s)" % (
                info.get("solver_status"), info.get("iters"), info.get("attempts")))
        else:
            show("solver", None)
        show("engngmodel", info.get("engngmodel"))
        show("oofem", "%s (%s)" % (info.get("oofem") or "-", info.get("git") or "-"))
        show("host", "%s / %s" % (info.get("host") or "-", info.get("pid") or "-"))
        show("start", info.get("start"))
        show("description", info.get("description"))
        frac = info.get("progress")
        if frac is None:
            self.progress.setRange(0, 0)  # busy indicator
        else:
            self.progress.setRange(0, 1000)
            self.progress.setValue(int(frac * 1000))
            self.progress.setFormat("%.1f %%" % (frac * 100))
        note = info.get("warning")
        if info.get("status") == "no file":
            note = "waiting for %s" % info.get("file")
        if note:
            self.win.statusBar().showMessage(note)

    def add_chart_view(self, spec):
        view = QtChartView(self, spec)
        self.views[spec.id] = view
        self.order.append(spec.id)
        self._relayout()
        return view

    def remove_chart_view(self, chart_id):
        view = self.views.pop(chart_id, None)
        if chart_id in self.order:
            self.order.remove(chart_id)
        if view is not None:
            view.box.setParent(None)
            view.box.deleteLater()
        self._relayout()

    def schedule(self, ms, callback):
        QtCore.QTimer.singleShot(ms, callback)

    def run(self):
        self.win.show()
        self.app.exec()

    def message(self, text):
        self.win.statusBar().showMessage(text, 5000)

    # ------------------------------------------------------------------ helpers and actions
    def _relayout(self):
        while self.grid.count():
            self.grid.takeAt(0)
        for i, chart_id in enumerate(self.order):
            row, col = divmod(i, self.columns)
            self.grid.addWidget(self.views[chart_id].box, row, col)
        for c in range(4):
            self.grid.setColumnStretch(c, 1 if c < self.columns else 0)

    def _set_columns(self, n):
        self.columns = n
        self._relayout()

    def apply_theme(self, dark):
        self.dark = bool(dark)
        self.app.setPalette(_dark_palette() if self.dark else self.light_palette)
        for view in self.views.values():
            view.apply_theme(self.dark)
        if self.theme_action.isChecked() != self.dark:
            self.theme_action.setChecked(self.dark)

    def add_chart_dialog(self):
        names = self.controller.series_names()
        dialog = AddChartDialog(self.win, names)
        if dialog.exec() == QtWidgets.QDialog.Accepted:
            try:
                self.controller.add_chart(dialog.spec())
            except ValueError as e:
                QtWidgets.QMessageBox.warning(self.win, "Add chart", str(e))

    def save_layout_dialog(self):
        path, _ = QtWidgets.QFileDialog.getSaveFileName(self.win, "Save layout", "progress-layout.json", "JSON (*.json)")
        if path:
            try:
                self.controller.save_layout(path)
            except OSError as e:
                QtWidgets.QMessageBox.warning(self.win, "Save layout", str(e))

    def load_layout_dialog(self):
        path, _ = QtWidgets.QFileDialog.getOpenFileName(self.win, "Load layout", "", "JSON (*.json)")
        if path:
            try:
                self.controller.load_layout(path)
            except (OSError, ValueError) as e:
                QtWidgets.QMessageBox.warning(self.win, "Load layout", str(e))
