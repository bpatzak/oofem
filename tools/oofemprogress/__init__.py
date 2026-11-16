"""Live viewer of OOFEM solution progress.

Follows the file written by OOFEM's ``progress`` export module (protocol ``oofem-progress``)
and shows job information, user defined charts (e.g. load-displacement diagram) and solver
convergence while the analysis runs. Run as ``python -m oofemprogress <file>``.

The viewer has several frontends sharing the same toolkit independent core
(:mod:`.protocol`, :mod:`.charts`, :mod:`.controller`):

* ``qt``  - PySide6 + pyqtgraph,
* ``tk``  - Tkinter + matplotlib,
* ``tui`` - terminal, standard library only (optionally rendered by plotext).
"""

__version__ = "1.0"
