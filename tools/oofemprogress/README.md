# oofemprogress – live monitor of OOFEM solution progress

`oofemprogress` follows the file written by OOFEM's `progress` export module while
the analysis runs. It shows:

- job information: job, OOFEM version and git hash, host, pid, start time, elapsed
  time, status, current step and progress bar,
- user-defined charts of any monitored quantity against any other, for example a
  load-displacement diagram,
- convergence of the nonlinear solver (error norms per iteration) for the current step.

OOFEM only appends lines to a file; the viewer is a separate process. The viewer can be
started before, during or after the run, and it detects when the job finishes, crashes
or is restarted.

## Usage

1. Add a `progress` export module to the OOFEM input (and increase `nmodules`), e.g.

   ```
   progress tstep_all iterations dofman_ids 1 2 dofman_dofs 1 1 reaction_dofs 1 1 reaction_sets 1 2 plot_x 1 1 plot_y 1 2
   ```

   This monitors the `D_u` displacement of node 2 and the sum of `D_u` reactions over
   the nodes of set 2, and suggests the reaction-displacement chart. See the
   *Solution progress export module* section of the OOFEM input manual for all
   parameters.

2. Run OOFEM and the viewer (from the `tools` directory, or with `tools` on `PYTHONPATH`):

   ```
   oofem -f job.in &
   python -m oofemprogress job.in        # or job.out, or job.out.progress.jsonl
   ```

A complete example is in [`examples/cyclic_tension.in`](examples/cyclic_tension.in).

### Frontends

| `--backend` | Requirements | Notes |
|---|---|---|
| `qt` | `PySide6`, `pyqtgraph` (`requirements-qt.txt`) | fast plots, zoom/pan, crosshair, dark theme |
| `tk` | `tkinter` (part of most Python installs), `matplotlib` (`requirements-tk.txt`) | matplotlib toolbar (zoom, pan, save image) |
| `tui` | none (standard library) | terminal; braille charts, `--ascii` for fonts without braille, `--tui-renderer plotext` if plotext 5 or 6 is installed (`requirements-tui.txt`; plotext 6 has a compiled kernel, plotext 5 is pure Python: use `plotext<6` where plotext 6 cannot be installed or loaded; without plotext the viewer falls back to braille) |

The default `--backend auto` uses the first available one in the order qt, tk, tui.
All frontends work on Linux and Windows 11. For the terminal frontend, use Windows
Terminal; in the legacy console, use `--ascii`.

Other options: `--chart X:Y[,Y2...]` (repeatable; `conv` for the convergence chart),
`--layout file.json` (chart layout saved from the viewer), `--poll MS`, `--no-color`,
`--list-backends`.

Terminal keys: `a` add chart, `c` convergence chart, `d` delete chart, `n`/`p` (or arrows)
next/previous chart, `g` grid of all charts, `s` save layout, `q` quit.

## Protocol `oofem-progress`, version 1

The file is [JSON Lines](https://jsonlines.org): UTF-8, one JSON object per line. OOFEM
writes each record as one complete line and flushes it, so a reader can tail the file.
Only the last line can be incomplete; a reader skips it until it is finished.
Non-finite numbers are written as `null`.

Every record has a `type`. Readers must ignore unknown record types and unknown fields;
incompatible changes increase `version`.

### `header`

Written once at the beginning of the file. A new header, or a truncated file, means a new run.

| field | meaning |
|---|---|
| `protocol`, `version` | `"oofem-progress"`, `1` |
| `oofem`, `git` | OOFEM version string and git hash of the build |
| `host`, `pid` | machine and process id of the solver (used to detect a crashed run) |
| `job`, `input`, `description` | output file name, input file name, problem description |
| `engngmodel` | class of the engineering model |
| `nsteps`, `final_time` | number of steps and target time of the analysis (progress estimate) |
| `start` | local start time, ISO 8601 |
| `iterations` | `true` if `iter` records are written |
| `channels` | list of monitored quantities, in the order of `step.values`, see below |
| `plots` | suggested charts: list of `{"x": id, "y": id}` (`"time"` is allowed) |
| `control` | reserved for steering the solver from the viewer (currently `null`) |

Each channel has a unique `id` and a `kind` with kind-specific fields:

| kind | fields |
|---|---|
| `dofman` | `dofman`, `dof` (e.g. `"D_u"`), `mode` (`"d"`, `"v"`, `"a"`) |
| `reaction` | `set` (0 = all supported nodes), `dof` |
| `loadlevel` | – |
| `ip` | `element`, `irule`, `gp` (1-based), `ist` (e.g. `"StressTensor"`), `comp` (1-based) |
| `agg` | `set`, `op` (`"max"`, `"min"`, `"avg"`), `ist`, `comp` |

### `step`

One per selected solution step:

```json
{"type":"step","step":3,"mstep":1,"time":0.3,"dt":0.1,"wall":12.4,
 "solver":{"status":"converged","iters":5,"attempts":2,"time":0.8},
 "values":[0.031,152.4,null]}
```

`wall` is the time elapsed since the start of the run, in seconds. `solver` describes the
last attempt to solve the step: `status` is one of `converged`, `diverged_its`,
`diverged_tol`, `failed` or `unknown`. `attempts` counts how many times the step was
solved, including restarts with a reduced time step. `time` is the solution time of the
step. `values` follow the order of `header.channels`; `null` means not available.

### `iter`

One per iteration of the nonlinear solver, only if `header.iterations` is `true`:

```json
{"type":"iter","step":3,"attempt":2,"iter":4,"status":"unknown","groups":["D_u","D_v"],
 "force_err":[1.2e-5,3.4e-6],"disp_err":[2.0e-6,null],"rtolf":1e-6,"rtold":1e-6,"wall":12.1}
```

The errors are the relative error norms per DOF group, as used by the solver's
convergence check. `groups` is `["all"]` when the solver does not group DOFs. `null`
means the error is not evaluated. On the last iteration of an attempt, `status` holds
the final reason (`converged`, `diverged_its` or `diverged_tol`). Earlier attempts of
the same step have a lower `attempt` number.

### `end`

Written when the analysis terminates normally:

```json
{"type":"end","status":"finished","wall":40.1,"warnings":0,"errors":0}
```

If the file has no `end` record and the process `pid` on `host` no longer exists, the
run crashed. On Windows, the viewer checks this with `OpenProcess`; it does not use
`os.kill`, which would terminate the process there.

## Tests

```
cd tools/oofemprogress && python -m pytest tests
```

The GUI tests are skipped when their dependencies are missing. The Qt test runs on the
offscreen platform.
