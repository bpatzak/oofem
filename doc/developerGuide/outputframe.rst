.. _OutputFrame:

Output and export modules
=========================

The results of the analysis are written by modules managed by the
``ExportModuleManager`` of the engineering model. Two kinds of modules are
distinguished:

* **Output modules** (derived from ``OutputModule``) report the *full state*
  of the model. Every component reports everything it knows; the module only
  selects the solution steps, dof managers and elements and the format of the
  output. Examples are the standard text output ``TextOutputModule``
  (keyword ``output``, the ``.out`` file) and ``JsonOutputModule``
  (``jsonoutput``).
* **Export modules** (derived directly from ``ExportModule``) export a selection
  of quantities chosen by the user, which they query from the model (for example
  through ``Element::giveIPValue``). Examples are the VTK modules, ``matlab``,
  ``hom`` and ``JsonExportModule`` (``json``).

``OutputModule`` derives from ``ExportModule``, so both kinds are declared in
the same input section and share the common services (time step selection,
initialization, termination). ``ExportModuleManager`` keeps output modules in a
separate list: they are processed before export modules
(``doOutputModulesOutput``, ``doExportModulesOutput``), and they do not
affect the numbering of export modules returned by ``giveModule``.

Standard text output
--------------------

Unless the output is suppressed (``suppress_output``), ``EngngModel`` creates an
implicit ``TextOutputModule`` in its ``initializeFrom`` method
(``EngngModel::createDefaultOutputModule``); engineering models overriding
``initializeFrom`` without calling the parent implementation must call it as
well. A ``TextOutputModule`` declared in the input replaces the implicit one.

The module owns the output file: it opens it on first request (writing the
header) and writes the footer in ``terminate``. ``EngngModel::giveOutputStream``
returns the stream of the module and ``EngngModel::hasTextOutput`` tells whether
there is one. The output of a solution step is produced by
``EngngModel::printOutputAt``, which can be overridden to add problem specific
data (reactions, eigenvalues, ...), and by the ``printOutputAt`` services of
dof managers, elements, integration points and material statuses.

Structured output
-----------------

Structured output is produced through the ``OutputWriter`` interface
(``outputwriter.h``), a streaming output driver. Components report their data
as records, named fields, lists and groups; the writer translates them into a
particular format. ``JsonLinesWriter`` writes every record as a single JSON
object on a separate line. Because the interface is a streaming one, no
intermediate containers are allocated by the components.

The data are reported by the ``reportOutput(OutputWriter &, TimeStep *)``
services:

* ``EngngModel::reportOutputAt`` loops over domains, dof managers and elements,
  opening a record (``dofman``, ``element``) for each of them,
* ``FEMComponent::reportOutput`` reports the fields of a component,
* ``GaussPoint::reportOutput`` and ``IntegrationPointStatus::reportOutput``
  report integration point data,
* ``EngngModel::reportDofOutputAt`` reports the unknowns of a DOF.

The default implementations of ``FEMComponent::reportOutput`` and
``IntegrationPointStatus::reportOutput`` report the text produced by the
``printOutputAt`` service (``reportLegacyOutput``, field ``text``), so every
class supports structured output from the beginning and classes can be
migrated one by one. ``EngngModel::reportDofOutputAt`` reports the values
printed by ``printDofOutputAt`` as named fields, so it works for all problems.

When implementing a new class:

* a dof manager, element or material status which overrides ``printOutputAt``
  should override ``reportOutput`` as well, reporting the same data as fields;
* a class derived from a class with structured output, which adds data to the
  text output, should report them in ``reportOutput`` too. As long as it does
  not, it should report the text of its standard output instead
  (``reportOutput`` calling ``reportLegacyOutput``), so that no information
  is lost. ``StructuralMaterialStatus`` does this automatically for derived
  statuses which do not override ``reportOutput``.

An example of the structured output of a material status:

.. code-block:: cpp

    void MyMaterialStatus :: reportOutput(OutputWriter &writer, TimeStep *tStep) const
    {
        StructuralMaterialStatus :: reportStrainsAndStresses(writer, tStep);
        writer.field("kappa", this->kappa);
        writer.field("damage", this->damage);
    }

The ``tools/jsonl_check.py`` script validates the JSON Lines output and
compares the DOF values with the text output of the same analysis; it is used
by the regression test ``tests/regression/sm/jsonoutput01.in``.
