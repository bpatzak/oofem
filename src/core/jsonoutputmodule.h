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

#ifndef jsonoutputmodule_h_
#define jsonoutputmodule_h_

#include "outputmodule.h"
#include "jsonfile.h"

///@name Input fields for JsonOutputModule
//@{
#define _IFT_JsonOutputModule_Name "jsonoutput"
#define _IFT_JsonOutputModule_nodeSets "node_sets"
#define _IFT_JsonOutputModule_elementSets "element_sets"
#define _IFT_JsonOutputModule_fileName "filename"
//@}

namespace oofem {
class JsonLinesWriter;

/**
 * Structured output of the full model state in JSON Lines format
 * (one JSON object per line, see JsonLinesWriter).
 *
 * The module reports the same information as the standard text output
 * (see TextOutputModule), using EngngModel::reportOutputAt and the reportOutput
 * services of individual components. Components not supporting structured output yet
 * report the text of their standard output (field "text").
 *
 * Time steps are selected by the standard export module keywords
 * (tstep_all, tstep_step, tsteps_out, ...), nodes and elements by optional sets
 * (node_sets, element_sets, one set number per domain, 0 means all).
 * The output is written into a single file, by default
 * <output file>.m<module number>.jsonl; the name can be set by the filename keyword.
 */
class OOFEM_EXPORT JsonOutputModule : public OutputModule
{
protected:
    /// Output file (stream, writer, file name).
    JsonFile file;
    /// Set which contains nodes which should be exported (one per domain).
    IntArray nodeSets;
    /// Set which contains elements which should be exported (one per domain).
    IntArray elementSets;

public:
    JsonOutputModule(int n, EngngModel *e);
    virtual ~JsonOutputModule() { }

    void initializeFrom(const std::shared_ptr<InputRecord> &ir) override;
    void initialize() override { }
    void doOutput(TimeStep *tStep, bool forcedOutput = false) override;
    void terminate() override;

    /// Returns the name of the output file.
    std :: string giveOutputFileName() { return file.giveFileName(); }

    const char *giveClassName() const override { return "JsonOutputModule"; }
    const char *giveInputRecordName() const { return _IFT_JsonOutputModule_Name; }
};
} // end namespace oofem
#endif // jsonoutputmodule_h_
