"""Smoke tests of the graphical frontends; skipped if their dependencies are missing.
The Qt test runs on the offscreen platform, the Tk test needs a display."""

import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from oofemprogress.controller import ViewerController  # noqa: E402
from oofemprogress.charts import ChartSpec  # noqa: E402

from test_protocol import HEADER, step, line  # noqa: E402


def write_run(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER) + line(step(1, 0.5, [0.1, 1.0])) + line(step(2, 1.0, [0.2, 3.0])) +
                     line({"type": "iter", "step": 2, "attempt": 1, "iter": 0, "groups": ["D_u"],
                           "force_err": [1e-2], "disp_err": [1e-3], "rtolf": 1e-6, "rtold": 1e-6}))
    return path


def test_qt_frontend(tmp_path, monkeypatch):
    monkeypatch.setenv("QT_QPA_PLATFORM", "offscreen")
    pytest.importorskip("PySide6")
    pytest.importorskip("pyqtgraph")
    from oofemprogress.fe_qt import QtFrontend, AddChartDialog
    fe = QtFrontend(theme="light")
    ctl = ViewerController(str(write_run(tmp_path)), fe)
    fe.build(ctl)
    ctl._rebuild_views()
    ctl.update()
    assert fe.fields["job"].text() == "j.out"
    assert sorted(fe.views) == [1, 2]
    items = fe.views[1].items
    assert list(items) == ["R_D_u"]
    dialog = AddChartDialog(fe.win, ctl.series_names())
    dialog.y.item(len(ctl.series_names()) - 1).setSelected(True)
    spec = ctl.add_chart(dialog.spec())
    assert spec.y == ["R_D_u"]
    fe.apply_theme(True)
    ctl.remove_chart(1)
    assert sorted(fe.views) == [2, spec.id]



def test_tk_frontend(tmp_path):
    tkinter = pytest.importorskip("tkinter")
    pytest.importorskip("matplotlib")
    try:
        root = tkinter.Tk()
        root.destroy()
    except tkinter.TclError:
        pytest.skip("no display")
    from oofemprogress.fe_tk import TkFrontend
    fe = TkFrontend()
    ctl = ViewerController(str(write_run(tmp_path)), fe)
    fe.build(ctl)
    ctl._rebuild_views()
    ctl.update()
    fe.flush()
    assert fe.info_vars["job"].get() == "j.out"
    spec = ctl.add_chart(ChartSpec(x="time", y=["u2_D_u"], logy=True))
    fe.flush()
    assert len(fe.figure.axes) == 3
    ctl.remove_chart(spec.id)
    fe.flush()
    assert len(fe.figure.axes) == 2
    fe.root.destroy()
