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

#ifndef textoutputmodule_h_
#define textoutputmodule_h_

#include <cstdio>
#include <string>

#include "outputmodule.h"

///@name Input fields for TextOutputModule
//@{
#define _IFT_TextOutputModule_Name "output"
#define _IFT_TextOutputModule_nodeSets "node_sets"
#define _IFT_TextOutputModule_elementSets "element_sets"
//@}

namespace oofem {
/**
 * Standard text output of OOFEM (the .out file).
 *
 * The module writes the main output file of the engineering model
 * (EngngModel::giveOutputBaseFileName()). It writes the header (on first access
 * to the stream), the output of each solution step and the footer (in terminate).
 *
 * Unless output is suppressed (suppress_output keyword of the engineering model),
 * an implicit (default) instance is created by the engineering model. The implicit
 * instance selects the time steps, nodes and elements using the OutputManager of
 * each domain, so that the output is identical to the traditional OOFEM output.
 *
 * The module can also be declared explicitly in the list of export modules
 * (keyword "output"). The explicit instance replaces the implicit one; time
 * steps are then selected by the standard export module keywords (tstep_all,
 * tstep_step, tsteps_out, ...) and nodes and elements by optional sets
 * (node_sets, element_sets, one set number per domain, 0 means all).
 *
 * At most one text output module can be defined for a problem.
 */
class OOFEM_EXPORT TextOutputModule : public OutputModule
{
protected:
    /// Output stream (opened on first request).
    FILE *outputStream;
    /// True for the default instance created by the engineering model.
    bool implicit;
    /// Set which contains nodes which should be exported (one per domain).
    IntArray nodeSets;
    /// Set which contains elements which should be exported (one per domain).
    IntArray elementSets;

public:
    TextOutputModule(int n, EngngModel *e, bool implicit = false);
    virtual ~TextOutputModule();

    void initializeFrom(const std::shared_ptr<InputRecord> &ir) override;
    void initialize() override { }

    /**
     * Returns the output stream. The stream is opened on first request
     * (and the output header is written).
     */
    FILE *giveOutputStream();
    /// Closes the stream; the next request reopens it (with current file name of the problem).
    void closeOutputStream();
    /// Returns true if the receiver is the default (implicit) instance.
    bool isImplicit() const { return implicit; }

    void doOutput(TimeStep *tStep, bool forcedOutput = false) override;
    void terminate() override;

    const char *giveClassName() const override { return "TextOutputModule"; }
    const char *giveInputRecordName() const { return _IFT_TextOutputModule_Name; }

protected:
    /// Writes the output header.
    void printHeader(FILE *file);
};
} // end namespace oofem
#endif // textoutputmodule_h_
