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

#include "jsonfile.h"
#include "outputwriter.h"
#include "engngm.h"
#include "timestep.h"
#include "error.h"

namespace oofem {
JsonFile :: JsonFile(EngngModel *emodel, int moduleNumber) : emodel(emodel), moduleNumber(moduleNumber), stream(nullptr)
{ }


JsonFile :: ~JsonFile()
{
    // the writer only references the stream, release it first
    writer.reset();
    if ( stream ) {
        fclose(stream);
    }
}


std :: string
JsonFile :: giveFileName() const
{
    if ( !fileName.empty() ) {
        return fileName;
    }
    std :: string name = emodel->giveOutputBaseFileName() + ".m" + std :: to_string(moduleNumber);
    if ( emodel->isParallel() && emodel->giveNumberOfProcesses() > 1 ) {
        // every process writes its own file
        name += "." + std :: to_string( emodel->giveRank() );
    }
    return name + ".jsonl";
}


JsonLinesWriter &
JsonFile :: giveWriter()
{
    if ( !writer ) {
        std :: string name = this->giveFileName();
        stream = fopen(name.c_str(), "w");
        if ( !stream ) {
            OOFEM_ERROR( "Can't open output file %s", name.c_str() );
        }
        writer = std :: make_unique< JsonLinesWriter >(stream);
        writer->beginOutput(emodel);
    }
    return * writer;
}


JsonLinesWriter &
JsonFile :: beginStep(TimeStep *tStep, double timeScale)
{
    JsonLinesWriter &w = this->giveWriter();
    w.beginStep(tStep, tStep->giveTargetTime() * emodel->giveVariableScale(VST_Time) * timeScale);
    return w;
}


void
JsonFile :: endStep()
{
    writer->endStep();
    fflush(stream);
}


void
JsonFile :: close()
{
    this->giveWriter().endOutput(emodel);
    writer.reset();
    fclose(stream);
    stream = nullptr;
}
} // end namespace oofem
