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
(``outputwriter.h``), a streaming output driver. The output is hierarchical:
steps (``beginStep``) contain domains (``beginDomain``), domains contain
sections of records (``beginSection``, e.g. ``dofmans``, ``elements``,
``reactions``), a record (``beginRecord``) is the output of one component and
contains named fields, lists and groups. The writer translates them into a
particular format. ``JsonLinesWriter`` writes every step as a single nested
JSON object on a separate line. Because the interface is a streaming one, no
intermediate containers are allocated by the components or by the writer.

The JSON modules share the output file handling (``JsonFile``: file name,
header and footer, step framing) and the traversal of the model:

* ``EngngModel::reportOutputAt(writer, tStep, filter, content)`` loops over
  domains, dof managers and elements, opening the domains, sections and a
  record for each component. The ``OutputFilter`` (``outputselection.h``)
  selects the domains, node and element sets and the sections; the
  ``OutputContent`` reports the fields of the records. The default content
  reports the full state of the components (``DofManager::reportOutput``,
  ``Element::reportOutput``), used by ``JsonOutputModule``;
  ``JsonExportModule`` uses its own content reporting only the selected DOFs
  and internal state types, so both modules produce the same structure.
* ``EngngModel::reportDomainOutputAt`` is called for each domain after the dof
  managers and elements; problems report their specific sections here, e.g.
  ``StructuralEngngModel`` reports the reaction forces
  (``reportReactionForces``, section ``reactions``) if requested by the
  filter.

Within the records, the data are reported by the
``reportOutput(OutputWriter &, TimeStep *)`` services:

* ``FEMComponent::reportOutput`` reports the fields of a component,
* ``Element::reportIntegrationPoints`` reports the integration rules of an
  element and their integration points (number, natural coordinates, weight),
  letting a given function report the data of each point; it is used by
  ``Element::reportOutput`` and by the json export, so the structure
  ``integration_rules`` → ``gps`` is the same,
* ``GaussPoint::reportOutput`` and ``IntegrationPointStatus::reportOutput``
  report integration point data,
* ``Dof::reportOutput`` reports a DOF (``id``, ``name``) and its unknowns,
  given by ``EngngModel::reportDofOutputAt`` (``Dof::reportOutputAt``).

The default implementations of ``FEMComponent::reportOutput`` and
``IntegrationPointStatus::reportOutput`` report the text produced by the
``printOutputAt`` service (``reportLegacyOutput``, field ``text``), so every
class supports structured output from the beginning and classes can be
migrated one by one.

``EngngModel::reportDofOutputAt`` is the structured counterpart of
``printDofOutputAt``: the default implementation reports the total value of the
unknown as the field ``d``. A problem overriding ``printDofOutputAt`` must
override ``reportDofOutputAt`` as well, reporting the same unknowns under the
names used in the text output, for example for a dynamic problem:

.. code-block:: cpp

    void MyDynamicProblem :: reportDofOutputAt(OutputWriter &writer, Dof *iDof, TimeStep *tStep)
    {
        iDof->reportOutputAt(writer, tStep, "d", VM_Total);
        iDof->reportOutputAt(writer, tStep, "v", VM_Velocity);
        iDof->reportOutputAt(writer, tStep, "a", VM_Acceleration);
    }

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

The ``tools/jsonl_check.py`` script validates the JSON Lines output (including
the nesting of the step lines) and compares the DOF values and reaction forces
with the text output of the same analysis; it is used by the regression tests
``tests/regression/sm/jsonoutput01.in`` and ``jsonexport01.in``.
