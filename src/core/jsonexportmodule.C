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

#include "jsonexportmodule.h"
#include "outputwriter.h"
#include "engngm.h"
#include "domain.h"
#include "dofmanager.h"
#include "dof.h"
#include "element.h"
#include "gausspoint.h"
#include "integrationrule.h"
#include "set.h"
#include "timestep.h"
#include "internalstatetype.h"
#include "dofiditem.h"
#include "classfactory.h"

namespace oofem {
REGISTER_ExportModule(JsonExportModule)

JsonExportModule :: JsonExportModule(int n, EngngModel *e) : ExportModule(n, e), file(e, n)
{ }


void
JsonExportModule :: initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    ExportModule :: initializeFrom(ir);

    dofs.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, dofs, _IFT_JsonExportModule_dofs);
    vars.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, vars, _IFT_JsonExportModule_vars);
    nodeSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, nodeSets, _IFT_JsonExportModule_nodeSets);
    elementSets.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, elementSets, _IFT_JsonExportModule_elementSets);
    std :: string fileName;
    IR_GIVE_OPTIONAL_FIELD(ir, fileName, _IFT_JsonExportModule_fileName);
    file.setFileName(fileName);
}


void
JsonExportModule :: exportDofManager(JsonLinesWriter &w, DofManager *dman, TimeStep *tStep)
{
    if ( dman->giveParallelMode() == DofManager_null ) {
        return;
    }
    w.beginRecord( "dofman", dman->giveNumber(), dman->giveLabel(), dman->giveClassName() );
    for ( int id : dofs ) {
        if ( dman->hasDofID( ( DofIDItem ) id ) ) {
            w.field( __DofIDItemToString( ( DofIDItem ) id ),
                     dman->giveDofWithID(id)->giveUnknown(VM_Total, tStep) );
        }
    }
    w.endRecord();
}


void
JsonExportModule :: exportElement(JsonLinesWriter &w, Element *elem, TimeStep *tStep)
{
    if ( elem->giveParallelMode() == Element_remote || !elem->isActivated(tStep) ) {
        return;
    }
    w.beginRecord( "element", elem->giveNumber(), elem->giveLabel(), elem->giveClassName() );
    w.beginList("gps");
    FloatArray value;
    for ( auto &iRule : elem->giveIntegrationRulesArray() ) {
        for ( auto &gp : *iRule ) {
            w.beginGroup();
            w.field( "ir", iRule->giveNumber() );
            w.field( "number", gp->giveNumber() );
            for ( int type : vars ) {
                if ( elem->giveIPValue(value, gp, ( InternalStateType ) type, tStep) ) {
                    w.field(__InternalStateTypeToString( ( InternalStateType ) type ), value);
                }
            }
            w.endGroup();
        }
    }
    w.endList();
    w.endRecord();
}


void
JsonExportModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    JsonLinesWriter &w = file.beginStep(tStep, timeScale);
    for ( int idomain = 1; idomain <= emodel->giveNumberOfDomains(); ++idomain ) {
        // all domains are exported unless restricted by domain_mask
        if ( !domainMask.isEmpty() && !testDomainOutput(idomain) ) {
            continue;
        }
        Domain *domain = emodel->giveDomain(idomain);
        int nset = nodeSets.giveSize() < idomain ? 0 : nodeSets.at(idomain);
        int eset = elementSets.giveSize() < idomain ? 0 : elementSets.at(idomain);
        w.beginDomain(idomain);

        if ( dofs.giveSize() ) {
            if ( nset == 0 ) {
                for ( auto &dman : domain->giveDofManagers() ) {
                    this->exportDofManager(w, dman.get(), tStep);
                }
            } else {
                for ( int inode : domain->giveSet(nset)->giveNodeList() ) {
                    this->exportDofManager(w, domain->giveDofManager(inode), tStep);
                }
            }
        }

        if ( vars.giveSize() ) {
            if ( eset == 0 ) {
                for ( auto &elem : domain->giveElements() ) {
                    this->exportElement(w, elem.get(), tStep);
                }
            } else {
                for ( int ielem : domain->giveSet(eset)->giveElementList() ) {
                    this->exportElement(w, domain->giveElement(ielem), tStep);
                }
            }
        }
        w.endDomain();
    }
    file.endStep();
}


void
JsonExportModule :: terminate()
{
    file.close();
}
} // end namespace oofem
