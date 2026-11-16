/*
 *
 *                 #####    #####   ######  ######  ###   ###
 *               ##   ##  ##   ##  ##      ##      ## ### ##
 *              ##   ##  ##   ##  ####    ####    ##  #  ##
 *             ##   ##  ##   ##  ##      ##      ##     ##
 *            ##   ##  ##   ##  ##      ##      ##     ##
 *            #####    #####   ##      ######  ##     ##
 *
 *
 *             OOFEM : Object Oriented Finite Element Code
 *
 *               Copyright (C) 1993 - 2025   Borek Patzak
 *
 *
 *
 *       Czech Technical University, Faculty of Civil Engineering,
 *   Department of Structural Mechanics, 166 29 Prague, Czech Republic
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef jsonexportmodule_h_
#define jsonexportmodule_h_

#include "exportmodule.h"
#include "jsonfile.h"
#include "outputselection.h"

///@name Input fields for JsonExportModule
//@{
#define _IFT_JsonExportModule_Name "json"
#define _IFT_JsonExportModule_dofs "dofs"
#define _IFT_JsonExportModule_vars "vars"
#define _IFT_JsonExportModule_nodeSets "node_sets"
#define _IFT_JsonExportModule_elementSets "element_sets"
#define _IFT_JsonExportModule_fileName "filename"
//@}

namespace oofem {

/**
 * Export of selected quantities in JSON Lines format (one JSON object per line,
 * see JsonLinesWriter).
 *
 * In contrast to JsonOutputModule (which reports the full state of the model),
 * this export module reports only the quantities selected by the user:
 * - dofs: list of DOF identifiers (DofIDItem values, e.g. 1 2 3 for D_u D_v D_w);
 *   their total values are reported for dof managers ("dofman" records),
 * - vars: list of internal state types (InternalStateType values, as in vtkxml);
 *   their values are reported for each integration point of elements ("element" records).
 *
 * Time steps are selected by the standard export module keywords
 * (tstep_all, tstep_step, tsteps_out, ...), nodes and elements by optional sets
 * (node_sets, element_sets, one set number per domain, 0 means all).
 * The output is written into a single file, by default
 * <output file>.m<module number>.jsonl; the name can be set by the filename keyword.
 */
class OOFEM_EXPORT JsonExportModule : public ExportModule
{
protected:
    /// Output file (stream, writer, file name).
    JsonFile file;
    /// DOFs to export.
    IntArray dofs;
    /// Internal state types to export.
    IntArray vars;
    /// Selection of exported domains, nodes and elements.
    OutputFilter filter;

public:
    JsonExportModule(int n, EngngModel *e);
    virtual ~JsonExportModule() { }

    void initializeFrom(const std::shared_ptr<InputRecord> &ir) override;
    void initialize() override { }
    void doOutput(TimeStep *tStep, bool forcedOutput = false) override;
    void terminate() override;

    /// Returns the name of the output file.
    std :: string giveOutputFileName() { return file.giveFileName(); }

    const char *giveClassName() const override { return "JsonExportModule"; }
    const char *giveInputRecordName() const { return _IFT_JsonExportModule_Name; }
};
} // end namespace oofem
#endif // jsonexportmodule_h_
