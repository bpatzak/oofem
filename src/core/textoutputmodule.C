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

#include <cstdio>
#include <ctime>

#include "oofemcfg.h"
#include "textoutputmodule.h"
#include "engngm.h"
#include "timestep.h"
#include "classfactory.h"

namespace oofem {
REGISTER_ExportModule(TextOutputModule)

TextOutputModule :: TextOutputModule(int n, EngngModel *e, bool implicit) : OutputModule(n, e),
    outputStream(nullptr), implicit(implicit)
{ }


TextOutputModule :: ~TextOutputModule()
{
    this->closeOutputStream();
}


void
TextOutputModule :: initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    nodeSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, nodeSets, _IFT_TextOutputModule_nodeSets);
    elementSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, elementSets, _IFT_TextOutputModule_elementSets);

    OutputModule :: initializeFrom(ir);
}


FILE *
TextOutputModule :: giveOutputStream()
{
    if ( this->outputStream ) {
        return this->outputStream;
    }

    this->outputStream = fopen(emodel->giveOutputBaseFileName().c_str(), "w");
    if ( !this->outputStream ) {
        OOFEM_ERROR("Can't open output file %s", emodel->giveOutputBaseFileName().c_str());
    }
    this->printHeader(this->outputStream);
    return this->outputStream;
}


void
TextOutputModule :: closeOutputStream()
{
    if ( this->outputStream ) {
        fclose(this->outputStream);
        this->outputStream = nullptr;
    }
}


void
TextOutputModule :: printHeader(FILE *file)
{
    fprintf(file, "%s", PRG_HEADER);
    fprintf(file, "\nStarting analysis on: %s\n", ctime(& emodel->giveStartTime()) );
    fprintf(file, "%s\n", emodel->giveDescription().c_str());

#ifdef __MPI_PARALLEL_MODE
    if ( emodel->isParallel() ) {
        fprintf(file, "Problem rank is %d/%d on %s\n\n", emodel->giveRank(), emodel->giveNumberOfProcesses(), emodel->giveProcessorName());
    }
#endif
}


void
TextOutputModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( this->implicit ) {
        // traditional output, steps, nodes and elements are selected by the output managers of domains
        FILE *file = this->giveOutputStream();
        emodel->printOutputAt(file, tStep);
        fflush(file);
        return;
    }

    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    FILE *file = this->giveOutputStream();

    fprintf(file, "\n==============================================================");
    fprintf(file, "\nOutput for time %.8e ", tStep->giveTargetTime() * emodel->giveVariableScale(VST_Time) );
    fprintf(file, "\n==============================================================\n");

    emodel->printOutputAt(file, tStep, nodeSets, elementSets);
    fflush(file);
}


void
TextOutputModule :: terminate()
{
    FILE *out = this->giveOutputStream();
    int rhrs, rmin, rsec, uhrs, umin, usec;
    time_t endTime = time(NULL);

    emodel->giveAnalysisTime(rhrs, rmin, rsec, uhrs, umin, usec);

    fprintf(out, "\nFinishing analysis on: %s\n", ctime(& endTime) );
    fprintf(out, "Real time consumed: %03dh:%02dm:%02ds\n", rhrs, rmin, rsec);
    fprintf(out, "User time consumed: %03dh:%02dm:%02ds\n\n\n", uhrs, umin, usec);
    fflush(out);
}
} // end namespace oofem
