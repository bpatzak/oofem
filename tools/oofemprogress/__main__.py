"""Command line entry point: ``python -m oofemprogress FILE [options]``."""

import argparse
import os
import sys

from .charts import Layout, parse_chart_option
from .controller import ViewerController
from .frontend import available_backends, create_frontend

SUFFIX = ".progress.jsonl"


def resolve_progress_file(name):
    """Maps the argument to the progress file.

    * ``*.progress.jsonl`` (or any existing file other than an input file) is used as is,
    * ``job.out`` -> ``job.out.progress.jsonl``,
    * ``job.in``  -> the output file named on the first line of the input file + suffix
      (the default file name of the progress export module).
    """
    if name.endswith(SUFFIX):
        return name
    if name.endswith(".in") and os.path.isfile(name):
        with open(name, encoding="utf-8", errors="replace") as f:
            first = f.readline().rstrip("\r\n")
        # OOFEM keeps the output file name verbatim (including trailing blanks)
        candidates = []
        for out in (first, first.strip()):
            candidates += [out + SUFFIX, os.path.join(os.path.dirname(name), out + SUFFIX)]
        for c in candidates:
            if os.path.exists(c):
                return c
        return first.strip() + SUFFIX
    if os.path.isfile(name):
        return name
    return name + SUFFIX


def main(argv=None):
    parser = argparse.ArgumentParser(
        prog="oofemprogress",
        description="Live viewer of OOFEM solution progress (file written by the 'progress' export module).")
    parser.add_argument("file", nargs="?", help="progress file, OOFEM output file name or input (.in) file")
    parser.add_argument("--backend", default="auto", choices=["auto", "qt", "tk", "tui"],
                        help="user interface (auto: qt, then tk, then tui)")
    parser.add_argument("--chart", action="append", default=[], metavar="X:Y[,Y2]",
                        help="chart to show (repeatable); 'conv' adds the solver convergence chart")
    parser.add_argument("--layout", help="chart layout (JSON) saved earlier from the viewer")
    parser.add_argument("--poll", type=int, default=500, metavar="MS", help="polling period in ms (default 500)")
    parser.add_argument("--tui-renderer", default="braille", choices=["braille", "ascii", "plotext"],
                        help="chart renderer of the terminal backend")
    parser.add_argument("--ascii", action="store_true", help="terminal backend: ASCII charts (same as --tui-renderer ascii)")
    parser.add_argument("--no-color", action="store_true", help="terminal backend: no colours")
    parser.add_argument("--list-backends", action="store_true", help="print available backends and exit")
    args = parser.parse_args(argv)

    if args.list_backends:
        print(" ".join(available_backends()))
        return 0
    if not args.file:
        parser.error("the progress file is required")

    layout = None
    try:
        if args.layout:
            layout = Layout.load(args.layout)
        if args.chart:
            layout = layout or Layout()
            for text in args.chart:
                layout.add(parse_chart_option(text))
    except (OSError, ValueError) as e:
        parser.error(str(e))

    renderer = "ascii" if args.ascii else args.tui_renderer
    frontend = create_frontend(args.backend, renderer=renderer, color=False if args.no_color else None)
    controller = ViewerController(resolve_progress_file(args.file), frontend, layout=layout, poll_ms=args.poll)
    controller.start()
    return 0


if __name__ == "__main__":
    sys.exit(main())
