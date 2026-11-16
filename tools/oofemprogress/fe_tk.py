"""Tk frontend: Tkinter (standard library) + matplotlib.

Fallback graphical viewer when PySide6 is not installed. All charts are axes of a single
matplotlib figure; the matplotlib navigation toolbar provides zoom, pan and export to image.
A chart zoomed by the user stops following the data until 'Autoscale' is pressed.
"""

import math
import tkinter as tk
from tkinter import ttk, filedialog, messagebox

import matplotlib
matplotlib.use("TkAgg")
from matplotlib.figure import Figure  # noqa: E402
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk  # noqa: E402
from cycler import cycler  # noqa: E402

from .frontend import Frontend, ChartView  # noqa: E402
from .charts import ChartSpec  # noqa: E402
from .protocol import format_duration  # noqa: E402

PALETTE = ("#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e",
           "#17becf", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22")
STATUS_COLORS = {"running": "#2ca02c", "finished": "#1f77b4", "crashed": "#d62728"}
STYLE = {
    "axes.grid": True, "grid.alpha": 0.3, "axes.spines.top": False, "axes.spines.right": False,
    "axes.titlesize": 10, "axes.titleweight": "bold", "axes.labelsize": 9, "xtick.labelsize": 8,
    "ytick.labelsize": 8, "legend.fontsize": 8, "legend.framealpha": 0.8, "lines.linewidth": 1.8,
    "axes.prop_cycle": cycler(color=PALETTE),
}


class TkChartView(ChartView):
    def __init__(self, frontend, spec):
        self.frontend = frontend
        self.spec = spec
        self.ax = None
        self.lines = {}
        self.curves = []
        self.title = spec.label()
        self.dirty = True
        self.auto_limits = None   # limits set by the last autoscale
        self.user_zoom = False

    def update(self, spec, curves, title):
        self.spec, self.curves, self.title = spec, curves, title
        self.dirty = True
        self.frontend.request_flush()

    def attach(self, ax):
        """Binds the view to (new) axes after the figure layout changed."""
        self.ax = ax
        self.lines = {}
        self.auto_limits = None
        self.user_zoom = False
        if self.spec.logx:
            ax.set_xscale("log")
        if self.spec.logy:
            ax.set_yscale("log")
        ax.set_xlabel("iteration" if self.spec.kind == "convergence" else self.spec.x)
        if self.spec.kind == "xy" and len(self.spec.y) == 1:
            ax.set_ylabel(self.spec.y[0])
        self.dirty = True

    def draw(self):
        ax = self.ax
        if ax is None:
            return
        if self.auto_limits is not None and (ax.get_xlim(), ax.get_ylim()) != self.auto_limits:
            self.user_zoom = True  # limits changed by the navigation toolbar
        seen = set()
        for i, c in enumerate(self.curves):
            line = self.lines.get(c.label)
            if line is None:
                (line,) = ax.plot([], [], label=c.label, color=PALETTE[i % len(PALETTE)],
                                  linestyle="--" if c.style == "dash" else "-",
                                  linewidth=1.2 if c.style == "dash" else None)
                self.lines[c.label] = line
            line.set_data(c.x, c.y)
            line.set_marker("o" if len(c.x) == 1 and c.style != "dash" else "")
            seen.add(c.label)
        for label in [k for k in self.lines if k not in seen]:
            self.lines.pop(label).remove()
        ax.set_title(self.title, loc="left")
        if self.lines:
            ax.legend(loc="best")
        elif ax.get_legend() is not None:
            ax.get_legend().remove()
        if not self.user_zoom and any(c.x for c in self.curves):
            ax.relim()
            ax.autoscale(enable=True)  # re-enables autoscaling switched off by toolbar zoom (set_xlim)
            self.auto_limits = (ax.get_xlim(), ax.get_ylim())
        self.dirty = False


class AddChartDialog(tk.Toplevel):
    def __init__(self, parent, names):
        super().__init__(parent)
        self.title("Add chart")
        self.transient(parent)
        self.result = None
        self.names = names
        frm = ttk.Frame(self, padding=10)
        frm.grid(sticky="nsew")
        ttk.Label(frm, text="Title").grid(row=0, column=0, sticky="w")
        self.title_var = tk.StringVar()
        ttk.Entry(frm, textvariable=self.title_var, width=40).grid(row=0, column=1, sticky="ew", pady=2)
        ttk.Label(frm, text="X axis").grid(row=1, column=0, sticky="w")
        self.x_var = tk.StringVar(value=names[0] if names else "")
        ttk.Combobox(frm, textvariable=self.x_var, values=names, state="readonly").grid(row=1, column=1, sticky="ew", pady=2)
        ttk.Label(frm, text="Y series").grid(row=2, column=0, sticky="nw")
        self.y = tk.Listbox(frm, selectmode=tk.EXTENDED, exportselection=False, height=min(12, max(4, len(names))))
        for n in names:
            self.y.insert(tk.END, n)
        self.y.grid(row=2, column=1, sticky="nsew", pady=2)
        self.logx = tk.BooleanVar()
        self.logy = tk.BooleanVar()
        ttk.Checkbutton(frm, text="logarithmic x", variable=self.logx).grid(row=3, column=1, sticky="w")
        ttk.Checkbutton(frm, text="logarithmic y", variable=self.logy).grid(row=4, column=1, sticky="w")
        buttons = ttk.Frame(frm)
        buttons.grid(row=5, column=1, sticky="e", pady=(8, 0))
        ttk.Button(buttons, text="OK", command=self.ok).pack(side=tk.LEFT, padx=4)
        ttk.Button(buttons, text="Cancel", command=self.destroy).pack(side=tk.LEFT)
        frm.columnconfigure(1, weight=1)
        self.bind("<Escape>", lambda e: self.destroy())

    def ok(self):
        ys = [self.names[i] for i in self.y.curselection()]
        if not ys:
            messagebox.showwarning("Add chart", "Select at least one Y series.", parent=self)
            return
        self.result = ChartSpec(x=self.x_var.get(), y=ys, title=self.title_var.get().strip(),
                                logx=self.logx.get(), logy=self.logy.get())
        self.destroy()


class TkFrontend(Frontend):
    name = "tk"

    def __init__(self, **_options):
        self.views = {}
        self.order = []
        self.columns = 2
        self._flush_pending = False

    # ------------------------------------------------------------------ building
    def build(self, controller):
        super().build(controller)
        matplotlib.rcParams.update(STYLE)
        self.root = tk.Tk()
        self.root.title("OOFEM progress - %s" % controller.path)
        self.root.geometry("1280x860")
        style = ttk.Style(self.root)
        for theme in ("vista", "aqua", "clam"):
            if theme in style.theme_names():
                style.theme_use(theme)
                break
        self._build_toolbar()
        self._build_info()
        self.figure = Figure(layout="constrained")
        self.canvas = FigureCanvasTkAgg(self.figure, master=self.root)
        self._keep_default_cursor()
        nav = NavigationToolbar2Tk(self.canvas, self.root, pack_toolbar=False)
        nav.update()
        nav.pack(side=tk.BOTTOM, fill=tk.X)
        self.status = ttk.Label(self.root, anchor="w")
        self.status.pack(side=tk.BOTTOM, fill=tk.X)
        self.canvas.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=True)

    def _keep_default_cursor(self):
        """Keeps the inherited (system) cursor over the charts instead of matplotlib's
        explicit 'arrow' and the 'watch' it shows while drawing.

        matplotlib switches to 'watch' and back to 'arrow' for every redraw that comes more
        than a second after the previous one, which happens on every update of a live viewer.
        Tk creates these cursors from the X cursor font; some X servers (notably WSLg on
        Windows) do not display them, so the cursor disappears after the first update.
        Zoom and pan keep matplotlib's cursors.
        """
        from matplotlib.backend_tools import Cursors
        widget = self.canvas.get_tk_widget()
        set_cursor = self.canvas.set_cursor

        def keep_default(cursor):
            if cursor in (Cursors.POINTER, Cursors.WAIT):
                widget.configure(cursor="")
            else:
                set_cursor(cursor)
        self.canvas.set_cursor = keep_default

    def _build_toolbar(self):
        bar = ttk.Frame(self.root, padding=(6, 4))
        bar.pack(side=tk.TOP, fill=tk.X)
        ttk.Button(bar, text="Add chart…", command=self.add_chart_dialog).pack(side=tk.LEFT)
        ttk.Button(bar, text="Convergence chart", command=self.controller.add_convergence_chart).pack(side=tk.LEFT, padx=4)
        ttk.Button(bar, text="Remove chart…", command=self.remove_chart_dialog).pack(side=tk.LEFT)
        ttk.Separator(bar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Button(bar, text="Save layout…", command=self.save_layout_dialog).pack(side=tk.LEFT)
        ttk.Button(bar, text="Load layout…", command=self.load_layout_dialog).pack(side=tk.LEFT, padx=4)
        ttk.Separator(bar, orient=tk.VERTICAL).pack(side=tk.LEFT, fill=tk.Y, padx=8)
        ttk.Label(bar, text="Columns").pack(side=tk.LEFT)
        self.columns_var = tk.IntVar(value=self.columns)
        ttk.Spinbox(bar, from_=1, to=4, width=3, textvariable=self.columns_var,
                    command=lambda: self._set_columns(self.columns_var.get())).pack(side=tk.LEFT, padx=4)
        ttk.Button(bar, text="Autoscale", command=self.autoscale).pack(side=tk.LEFT, padx=4)

    def _build_info(self):
        box = ttk.Frame(self.root, padding=(8, 2))
        box.pack(side=tk.TOP, fill=tk.X)
        self.info_vars = {}
        items = [("job", "Job"), ("status", "Status"), ("step", "Step"), ("time", "Time"),
                 ("elapsed", "Elapsed"), ("solver", "Last step"), ("engngmodel", "Problem"),
                 ("oofem", "OOFEM"), ("host", "Host / PID"), ("start", "Started")]
        for i, (key, label) in enumerate(items):
            row, col = divmod(i, 4)
            ttk.Label(box, text=label + ":", foreground="gray").grid(row=row, column=2 * col, sticky="w", padx=(0, 4))
            var = tk.StringVar(value="-")
            widget = ttk.Label(box, textvariable=var)
            widget.grid(row=row, column=2 * col + 1, sticky="w", padx=(0, 16))
            self.info_vars[key] = var
            if key == "status":
                self.status_label = widget
        ttk.Label(box, text="Description:", foreground="gray").grid(row=3, column=0, sticky="w")
        self.info_vars["description"] = tk.StringVar(value="-")
        ttk.Label(box, textvariable=self.info_vars["description"]).grid(row=3, column=1, columnspan=7, sticky="w")
        self.progress = ttk.Progressbar(box, maximum=1000)
        self.progress.grid(row=4, column=0, columnspan=8, sticky="ew", pady=(4, 2))
        box.columnconfigure(7, weight=1)

    # ------------------------------------------------------------------ Frontend interface
    def set_info(self, info):
        def show(key, text):
            self.info_vars[key].set("-" if text is None or text == "" else str(text))
        show("job", info.get("job"))
        status = info.get("status") or "-"
        show("status", status.upper())
        self.status_label.configure(foreground=STATUS_COLORS.get(status, "#d08000"))
        show("step", "%s / %s" % (info.get("step") if info.get("step") is not None else "-", info.get("nsteps") or "?"))
        t, tf = info.get("time"), info.get("final_time")
        show("time", "%s / %s" % ("-" if t is None else "%.6g" % t, "?" if tf is None else "%.6g" % tf))
        show("elapsed", format_duration(info.get("elapsed")))
        show("solver", "%s, %s iterations, %s attempt(s)" % (info.get("solver_status"), info.get("iters"),
                                                             info.get("attempts")) if info.get("solver_status") else None)
        show("engngmodel", info.get("engngmodel"))
        show("oofem", "%s (%s)" % (info.get("oofem") or "-", info.get("git") or "-"))
        show("host", "%s / %s" % (info.get("host") or "-", info.get("pid") or "-"))
        show("start", info.get("start"))
        show("description", info.get("description"))
        frac = info.get("progress")
        if frac is None:
            if str(self.progress.cget("mode")) != "indeterminate":
                self.progress.configure(mode="indeterminate")
                self.progress.start(50)
        else:
            if str(self.progress.cget("mode")) != "determinate":
                self.progress.stop()
                self.progress.configure(mode="determinate")
            self.progress["value"] = frac * 1000
        note = info.get("warning")
        if info.get("status") == "no file":
            note = "waiting for %s" % info.get("file")
        if note:
            self.status.configure(text=note)

    def add_chart_view(self, spec):
        view = TkChartView(self, spec)
        self.views[spec.id] = view
        self.order.append(spec.id)
        self._relayout()
        return view

    def remove_chart_view(self, chart_id):
        self.views.pop(chart_id, None)
        if chart_id in self.order:
            self.order.remove(chart_id)
        self._relayout()

    def schedule(self, ms, callback):
        self.root.after(ms, callback)

    def run(self):
        self.root.mainloop()

    def message(self, text):
        self.status.configure(text=text)
        self.root.after(5000, lambda: self.status.configure(text=""))

    # ------------------------------------------------------------------ drawing
    def request_flush(self):
        if not self._flush_pending:
            self._flush_pending = True
            self.root.after_idle(self.flush)

    def flush(self):
        """Redraws changed charts (coalesced after a batch of updates)."""
        self._flush_pending = False
        changed = False
        for view in self.views.values():
            if view.dirty:
                view.draw()
                changed = True
        if changed:
            self.canvas.draw_idle()

    def _relayout(self):
        self.figure.clear()
        n = len(self.order)
        if n:
            ncols = min(self.columns, n)
            nrows = math.ceil(n / ncols)
            for i, chart_id in enumerate(self.order):
                self.views[chart_id].attach(self.figure.add_subplot(nrows, ncols, i + 1))
        self.request_flush()
        self.canvas.draw_idle()

    def _set_columns(self, n):
        self.columns = max(1, int(n))
        self._relayout()

    def autoscale(self):
        for view in self.views.values():
            view.user_zoom = False
            view.auto_limits = None
            view.dirty = True
        self.flush()

    # ------------------------------------------------------------------ dialogs
    def add_chart_dialog(self):
        dialog = AddChartDialog(self.root, self.controller.series_names())
        dialog.grab_set()
        self.root.wait_window(dialog)
        if dialog.result is not None:
            try:
                self.controller.add_chart(dialog.result)
            except ValueError as e:
                messagebox.showwarning("Add chart", str(e), parent=self.root)

    def remove_chart_dialog(self):
        if not self.order:
            return
        top = tk.Toplevel(self.root)
        top.title("Remove chart")
        top.transient(self.root)
        lb = tk.Listbox(top, width=50, height=min(10, len(self.order)))
        for chart_id in self.order:
            lb.insert(tk.END, self.views[chart_id].title)
        lb.pack(padx=10, pady=10, fill=tk.BOTH, expand=True)

        def remove():
            sel = lb.curselection()
            if sel:
                self.controller.remove_chart(self.order[sel[0]])
            top.destroy()
        ttk.Button(top, text="Remove", command=remove).pack(pady=(0, 10))
        top.grab_set()

    def save_layout_dialog(self):
        path = filedialog.asksaveasfilename(parent=self.root, title="Save layout", defaultextension=".json",
                                            initialfile="progress-layout.json", filetypes=[("JSON", "*.json")])
        if path:
            try:
                self.controller.save_layout(path)
            except OSError as e:
                messagebox.showwarning("Save layout", str(e), parent=self.root)

    def load_layout_dialog(self):
        path = filedialog.askopenfilename(parent=self.root, title="Load layout", filetypes=[("JSON", "*.json")])
        if path:
            try:
                self.controller.load_layout(path)
            except (OSError, ValueError) as e:
                messagebox.showwarning("Load layout", str(e), parent=self.root)
