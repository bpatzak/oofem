import json
import os
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from oofemprogress.controller import ViewerController  # noqa: E402
from oofemprogress.frontend import Frontend, ChartView  # noqa: E402
from oofemprogress.charts import ChartSpec, Layout  # noqa: E402

from test_protocol import HEADER, step, line  # noqa: E402


class FakeView(ChartView):
    def __init__(self):
        self.draws = []

    def update(self, spec, curves, title):
        self.draws.append((title, [(c.label, list(c.x), list(c.y)) for c in curves]))


class FakeFrontend(Frontend):
    """Frontend without GUI: records what the controller asks it to do."""
    name = "fake"

    def __init__(self):
        self.views = {}
        self.removed = []
        self.info = None
        self.scheduled = []
        self.messages = []

    def set_info(self, info):
        self.info = info

    def add_chart_view(self, spec):
        self.views[spec.id] = FakeView()
        return self.views[spec.id]

    def remove_chart_view(self, chart_id):
        self.removed.append(chart_id)
        self.views.pop(chart_id, None)

    def schedule(self, ms, callback):
        self.scheduled.append((ms, callback))

    def run(self):
        pass

    def message(self, text):
        self.messages.append(text)


def test_controller_follows_file(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(b"")
    fe = FakeFrontend()
    ctl = ViewerController(str(path), fe)
    ctl.start()
    assert fe.info["status"] == "waiting"
    assert fe.scheduled  # polling rescheduled

    path.write_bytes(line(HEADER) + line(step(1, 0.5, [0.1, 1.0])))
    ctl.update()
    # default layout from header: x-y chart + convergence chart
    assert sorted(fe.views) == [1, 2]
    title, curves = fe.views[1].draws[-1]
    assert curves == [("R_D_u", [0.1], [1.0])]
    assert fe.info["job"] == "j.out"
    assert fe.info["step"] == 1

    # no change -> no redraw
    n = len(fe.views[1].draws)
    ctl.update()
    assert len(fe.views[1].draws) == n


def test_user_charts_and_layout_io(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER) + line(step(1, 0.5, [0.1, 1.0])) + line(step(2, 1.0, [0.2, 2.0])))
    fe = FakeFrontend()
    ctl = ViewerController(str(path), fe)
    ctl.start()
    spec = ctl.add_chart(ChartSpec(x="time", y=["u2_D_u", "R_D_u"]))
    assert fe.views[spec.id].draws[-1][1][1] == ("R_D_u", [0.5, 1.0], [1.0, 2.0])
    with pytest.raises(ValueError):
        ctl.add_chart(ChartSpec(x="time", y=["nonexistent"]))

    ctl.remove_chart(1)
    assert 1 in fe.removed and 1 not in fe.views

    lpath = tmp_path / "layout.json"
    ctl.save_layout(str(lpath))
    assert json.loads(lpath.read_text())["charts"][-1]["y"] == ["u2_D_u", "R_D_u"]

    # user layout survives a new run (new header)
    path.write_bytes(line(dict(HEADER, job="second.out")))
    ctl.update()
    assert [c.y for c in ctl.layout.charts if c.kind == "xy"] == [["u2_D_u", "R_D_u"]]

    ctl.load_layout(str(lpath))
    assert len(ctl.layout.charts) == 2


def test_explicit_layout_is_kept(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER))
    fe = FakeFrontend()
    ctl = ViewerController(str(path), fe, layout=Layout([ChartSpec(x="time", y=["R_D_u"])]))
    ctl.start()
    assert len(ctl.layout.charts) == 1
