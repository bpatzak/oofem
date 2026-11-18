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

#include "jsonoutputmodule.h"
#include "outputwriter.h"
#include "outputselection.h"
#include "engngm.h"
#include "timestep.h"
#include "classfactory.h"

namespace oofem {
REGISTER_ExportModule(JsonOutputModule)

JsonOutputModule :: JsonOutputModule(int n, EngngModel *e) : OutputModule(n, e), file(e, n)
{ }


void
JsonOutputModule :: initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    OutputModule :: initializeFrom(ir);

    filter = OutputFilter();
    IR_GIVE_OPTIONAL_FIELD(ir, filter.nodeSets, _IFT_JsonOutputModule_nodeSets);
    IR_GIVE_OPTIONAL_FIELD(ir, filter.elementSets, _IFT_JsonOutputModule_elementSets);
    // all domains are reported unless restricted by domain_mask
    if ( !domain_all_flag ) {
        filter.domains = domainMask;
    }
    std :: string fileName;
    IR_GIVE_OPTIONAL_FIELD(ir, fileName, _IFT_JsonOutputModule_fileName);
    file.setFileName(fileName);
}


void
JsonOutputModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    JsonLinesWriter &w = file.beginStep(tStep, timeScale);
    // full state of the components (default content)
    OutputContent content;
    emodel->reportOutputAt(w, tStep, filter, content);
    file.endStep();
}


void
JsonOutputModule :: terminate()
{
    file.close();
}
} // end namespace oofem
