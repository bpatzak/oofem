import json
import os
import sys

sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))

from oofemprogress.protocol import ProgressReader, RunState, is_process_alive  # noqa: E402
from oofemprogress.charts import Layout, ChartSpec, chart_curves, parse_chart_option  # noqa: E402

HEADER = {"type": "header", "protocol": "oofem-progress", "version": 1, "job": "j.out",
          "pid": 0, "host": "", "nsteps": 4, "final_time": 2.0, "iterations": True,
          "channels": [{"id": "u2_D_u", "kind": "dofman"}, {"id": "R_D_u", "kind": "reaction"}],
          "plots": [{"x": "u2_D_u", "y": "R_D_u"}]}


def line(rec):
    return (json.dumps(rec) + "\n").encode()


def step(n, t, values, status="converged"):
    return {"type": "step", "step": n, "time": t, "wall": 0.1 * n,
            "solver": {"status": status, "iters": 3, "attempts": 1}, "values": values}


def test_partial_line_is_buffered(tmp_path):
    path = tmp_path / "p.jsonl"
    data = line(HEADER) + line(step(1, 0.5, [1, 2]))
    path.write_bytes(data[:-10])
    reader = ProgressReader(str(path))
    recs = reader.poll()
    assert [r["type"] for r in recs] == ["header"]
    path.write_bytes(data)
    recs = reader.poll()
    assert [r["type"] for r in recs] == ["step"]
    assert reader.poll() == []


def test_crlf_and_garbage_lines(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER).replace(b"\n", b"\r\n") + b"not json\n" + line(step(1, 0.5, [1, 2])))
    reader = ProgressReader(str(path))
    recs = reader.poll()
    assert [r["type"] for r in recs] == ["header", "step"]
    assert reader.bad_lines == 1


def test_truncation_and_rewrite_reset(tmp_path):
    path = tmp_path / "p.jsonl"
    path.write_bytes(line(HEADER) + line(step(1, 0.5, [1, 2])) + line(step(2, 1.0, [2, 3])))
    reader = ProgressReader(str(path))
    state = RunState()
    state.apply_all(reader.poll())
    assert state.nsteps_done() == 2
    # new run: shorter file
    path.write_bytes(line(HEADER))
    recs = reader.poll()
    assert recs[0]["type"] == "_reset"
    state.apply_all(recs)
    assert state.nsteps_done() == 0
    # new run that grew beyond the old position before the next poll: detected by changed header
    h2 = dict(HEADER, job="other.out")
    path.write_bytes(line(h2) + line(step(1, 0.5, [1, 2])) * 5)
    recs = reader.poll()
    assert recs[0]["type"] == "_reset"
    state.apply_all(recs)
    assert state.header["job"] == "other.out"
    assert state.nsteps_done() == 5


def test_missing_file(tmp_path):
    reader = ProgressReader(str(tmp_path / "none.jsonl"))
    assert reader.poll() == []
    assert not reader.exists()


def test_state_series_progress_and_unknown_records():
    state = RunState()
    assert state.status() == "waiting"
    state.apply(HEADER)
    state.apply({"type": "future_record", "x": 1})
    state.apply(step(1, 0.5, [0.1, None]))
    state.apply(step(2, 1.0, [0.2, 5.0]))
    assert state.series["u2_D_u"] == [0.1, 0.2]
    assert state.series["R_D_u"] == [None, 5.0]
    assert state.series["time"] == [0.5, 1.0]
    assert state.progress() == 0.5
    state.apply({"type": "end", "status": "finished", "wall": 3.0})
    assert state.status() == "finished"
    assert state.progress() == 1.0
    assert state.elapsed() == 3.0


def test_newer_protocol_version_warns():
    state = RunState()
    state.apply(dict(HEADER, version=99))
    assert "newer" in state.info()["warning"]


def test_crash_detection_with_dead_pid():
    state = RunState()
    # pid of a process that surely does not exist on this host
    state.apply(dict(HEADER, pid=2 ** 22 + 12345, host=__import__("socket").gethostname()))
    assert state.status() == "crashed"


def test_process_alive_checks():
    assert is_process_alive(os.getpid(), __import__("socket").gethostname()) is True
    assert is_process_alive(os.getpid(), "some-other-host.example") is None
    assert is_process_alive(0) is None


def test_iterations_and_convergence_curves():
    state = RunState()
    state.apply(HEADER)
    for attempt in (1, 2):
        for it, err in enumerate([1e-1, 1e-3, 1e-8]):
            state.apply({"type": "iter", "step": 3, "attempt": attempt, "iter": it, "groups": ["D_u", "D_v"],
                         "force_err": [err, None], "disp_err": [err * 2, err], "rtolf": 1e-6, "rtold": 1e-6})
    assert state.last_iter_key == (3, 2)
    curves = chart_curves(ChartSpec(kind="convergence", logy=True), state)
    labels = [c.label for c in curves]
    assert labels == ["F D_u", "D D_u", "D D_v", "rtolf", "rtold"]
    assert curves[0].x == [0, 1, 2]


def test_xy_curves_skip_missing_and_nonpositive_on_log():
    state = RunState()
    state.apply(HEADER)
    state.apply(step(1, 0.0, [0.0, 1.0]))
    state.apply(step(2, 0.5, [0.1, None]))
    state.apply(step(3, 1.0, [0.2, 3.0]))
    c = chart_curves(ChartSpec(x="u2_D_u", y=["R_D_u"]), state)[0]
    assert (c.x, c.y) == ([0.0, 0.2], [1.0, 3.0])
    c = chart_curves(ChartSpec(x="u2_D_u", y=["R_D_u"], logx=True), state)[0]
    assert (c.x, c.y) == ([0.2], [3.0])


def test_default_layout_and_roundtrip(tmp_path):
    state = RunState()
    state.apply(HEADER)
    layout = Layout.default_for(state)
    assert [(c.kind, c.x, c.y) for c in layout.charts] == [("xy", "u2_D_u", ["R_D_u"]), ("convergence", "time", [])]
    path = tmp_path / "layout.json"
    layout.save(str(path))
    loaded = Layout.load(str(path))
    assert [c.to_dict() for c in loaded.charts] == [c.to_dict() for c in layout.charts]
    assert [c.id for c in loaded.charts] == [1, 2]


def test_parse_chart_option():
    s = parse_chart_option("u2_D_u:R_D_u, lambda")
    assert (s.x, s.y) == ("u2_D_u", ["R_D_u", "lambda"])
    assert parse_chart_option("conv").kind == "convergence"
