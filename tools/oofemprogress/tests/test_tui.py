import io
import os
import re
import sys

import pytest

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from oofemprogress import tui_canvas  # noqa: E402
from oofemprogress.charts import Curve  # noqa: E402
from oofemprogress.controller import ViewerController  # noqa: E402
from oofemprogress.fe_tui import TuiFrontend  # noqa: E402

from test_protocol import HEADER, step, line  # noqa: E402

ANSI = re.compile(r"\x1b\[[0-9;?]*[A-Za-z]")


@pytest.mark.parametrize("mode", ["braille", "ascii"])
@pytest.mark.parametrize("color", [True, False])
def test_render_chart_has_exact_size(mode, color):
    curves = [Curve("a", [0, 1, 2, 3], [0, 1, 4, 9]), Curve("tol", [0, 3], [2, 2], style="dash")]
    lines = tui_canvas.render_chart(curves, 60, 15, "title", "x", mode=mode, use_color=color)
    assert len(lines) == 15
    assert all(tui_canvas.visible_len(s) == 60 for s in lines)
    plain = "\n".join(ANSI.sub("", s) for s in lines)
    assert "title" in plain and "[x]" in plain
    if mode == "ascii":
        assert all(ord(ch) < 128 for ch in plain)
    else:
        assert any(0x2800 < ord(ch) <= 0x28FF for ch in plain)


def test_render_chart_edge_cases():
    # no data, single point, constant values, log scale
    assert "(no data)" in "".join(tui_canvas.render_chart([], 40, 10, mode="ascii", use_color=False))
    lines = tui_canvas.render_chart([Curve("p", [1.0], [5.0])], 40, 10, mode="ascii", use_color=False)
    assert any("*" in s for s in lines)
    lines = tui_canvas.render_chart([Curve("c", [0, 1, 2], [3, 3, 3])], 40, 10, use_color=False)
    assert len(lines) == 10
    lines = tui_canvas.render_chart([Curve("e", [0, 1, 2], [1e-1, 1e-4, 1e-8])], 40, 10, logy=True, use_color=False)
    assert "1e-08" in "".join(lines)


def test_tick_noise_is_shown_as_zero():
    lines = tui_canvas.render_chart([Curve("r", [0, 1, 2], [-8.87e-33, 50, 118])], 50, 12, use_color=False)
    assert "e-33" not in "".join(lines)


def test_pad_truncates_text_with_escapes():
    text = "\x1b[31m" + "x" * 30 + "\x1b[0m"
    assert tui_canvas.visible_len(tui_canvas.pad(text, 10)) == 10


def test_tui_frame(tmp_path, monkeypatch):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER) + line(step(1, 0.5, [0.1, 1.0])) + line(step(2, 1.0, [0.2, 3.0])))
    monkeypatch.setenv("COLUMNS", "100")
    monkeypatch.setenv("LINES", "30")
    fe = TuiFrontend(color=False)
    ctl = ViewerController(str(path), fe)
    fe.build(ctl)
    ctl._rebuild_views()
    ctl.update()
    buf = io.StringIO()
    monkeypatch.setattr(sys, "stdout", buf)
    fe.draw()
    fe.handle_key("g")
    fe.draw()
    frames = [ANSI.sub("", f) for f in buf.getvalue().split("\x1b[H")[1:]]
    assert "job: j.out" in frames[0]
    assert "[1/2] R_D_u vs u2_D_u" in frames[0]
    assert "[2/2]" in frames[1]  # grid shows both charts
    assert all(len(f.split("\n")) <= 29 for f in frames)


def test_plotext_renderer():
    """plotext renderer, with whichever plotext (5 or 6) is installed; skipped without plotext."""
    pytest.importorskip("plotext")
    try:
        from oofemprogress.tui_plotext import render_chart, PLOTEXT_API
    except ImportError as e:
        pytest.skip("plotext renderer not available: %s" % e)
    assert PLOTEXT_API in (5, 6)
    xy = [Curve("F", [0, 1, 2, 3], [0, 50, 120, 60])]
    conv = [Curve("F D_u", [0, 1, 2], [1e-1, 1e-4, 1e-8]),
            Curve("rtolf", [0, 2], [1e-6, 1e-6], style="dash")]
    only_tolerance = [Curve("rtolf", [0, 1], [1e-7, 1e-7], style="dash")]  # zero range on log axis
    for curves, logy in ((xy, False), (conv, True), ([], True), (only_tolerance, True)):
        for color in (True, False):
            lines = render_chart(curves, 70, 18, "title", "x", False, logy, color)
            assert len(lines) == 18
            assert all(tui_canvas.visible_len(s) == 70 for s in lines)
    plain = [ANSI.sub("", s) for s in render_chart(conv, 70, 18, "t", "it", False, True, False)]
    dotted = [i for i, s in enumerate(plain) if sum(s.count(ch) for ch in "\u00b7\u2022\u22c5.") > 5]
    assert dotted, "tolerance line missing"
    # 1e-6 lies at 5/7 of the log range 1e-1..1e-8 from the top, i.e. in the lower half
    assert dotted[0] > len(plain) // 2, "tolerance line placed incorrectly on log axis"
