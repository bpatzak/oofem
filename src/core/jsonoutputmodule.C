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
#include "engngm.h"
#include "timestep.h"
#include "classfactory.h"

namespace oofem {
REGISTER_ExportModule(JsonOutputModule)

JsonOutputModule :: JsonOutputModule(int n, EngngModel *e) : OutputModule(n, e), stream(nullptr)
{ }


JsonOutputModule :: ~JsonOutputModule()
{
    writer.reset();
    if ( stream ) {
        fclose(stream);
    }
}


void
JsonOutputModule :: initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    OutputModule :: initializeFrom(ir);

    nodeSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, nodeSets, _IFT_JsonOutputModule_nodeSets);
    elementSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, elementSets, _IFT_JsonOutputModule_elementSets);
    fileName.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, fileName, _IFT_JsonOutputModule_fileName);
}


std :: string
JsonOutputModule :: giveOutputFileName()
{
    if ( !fileName.empty() ) {
        return fileName;
    }
    std :: string name = emodel->giveOutputBaseFileName() + ".m" + std :: to_string(this->number);
    if ( emodel->isParallel() && emodel->giveNumberOfProcesses() > 1 ) {
        name += "." + std :: to_string( emodel->giveRank() );
    }
    return name + ".jsonl";
}


JsonLinesWriter &
JsonOutputModule :: giveWriter()
{
    if ( !writer ) {
        std :: string name = this->giveOutputFileName();
        stream = fopen(name.c_str(), "w");
        if ( !stream ) {
            OOFEM_ERROR( "Can't open output file %s", name.c_str() );
        }
        writer = std :: make_unique< JsonLinesWriter >(stream);
        writer->beginOutput(emodel);
    }
    return * writer;
}


void
JsonOutputModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    JsonLinesWriter &w = this->giveWriter();
    w.beginStep(tStep, tStep->giveTargetTime() * emodel->giveVariableScale(VST_Time) * timeScale);
    emodel->reportOutputAt(w, tStep, nodeSets, elementSets);
    w.endStep();
    fflush(stream);
}


void
JsonOutputModule :: terminate()
{
    this->giveWriter().endOutput(emodel);
    writer.reset();
    fclose(stream);
    stream = nullptr;
}
} // end namespace oofem
