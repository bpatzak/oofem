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
#include "outputselection.h"
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

namespace {
/**
 * Content of the json export: instead of the full state of the components,
 * only the values of the selected DOFs and internal state types are reported.
 */
class SelectedContent : public OutputContent
{
protected:
    /// DOFs to export.
    const IntArray &dofs;
    /// Internal state types to export.
    const IntArray &vars;

public:
    SelectedContent(const IntArray &dofs, const IntArray &vars) : dofs(dofs), vars(vars) { }

    void reportDofManager(OutputWriter &w, DofManager *dman, TimeStep *tStep) override
    {
        for ( int id : dofs ) {
            if ( dman->hasDofID( ( DofIDItem ) id ) ) {
                w.field( __DofIDItemToString( ( DofIDItem ) id ),
                         dman->giveDofWithID(id)->giveUnknown(VM_Total, tStep) );
            }
        }
    }

    void reportElement(OutputWriter &w, Element *elem, TimeStep *tStep) override
    {
        bool active = elem->isActivated(tStep);
        w.field("active", active);
        if ( !active ) {
            return;
        }
        // same structure as the full output, the points report the selected internal state types
        FloatArray value;
        elem->reportIntegrationPoints(w, tStep, [this, &w, &value, elem, tStep] (GaussPoint *gp) {
            for ( int type : vars ) {
                if ( elem->giveIPValue(value, gp, ( InternalStateType ) type, tStep) ) {
                    w.field(__InternalStateTypeToString( ( InternalStateType ) type ), value);
                }
            }
        });
    }
};
} // end anonymous namespace

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
    filter = OutputFilter();
    IR_GIVE_OPTIONAL_FIELD(ir, filter.nodeSets, _IFT_JsonExportModule_nodeSets);
    IR_GIVE_OPTIONAL_FIELD(ir, filter.elementSets, _IFT_JsonExportModule_elementSets);
    // all domains are exported unless restricted by domain_mask
    if ( !domain_all_flag ) {
        filter.domains = domainMask;
    }
    // sections without selected quantities are not exported
    filter.dofManagers = dofs.giveSize() > 0;
    filter.elements = vars.giveSize() > 0;
    filter.reactions = false;
    std :: string fileName;
    IR_GIVE_OPTIONAL_FIELD(ir, fileName, _IFT_JsonExportModule_fileName);
    file.setFileName(fileName);
}


void
JsonExportModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    JsonLinesWriter &w = file.beginStep(tStep, timeScale);
    SelectedContent content(dofs, vars);
    emodel->reportOutputAt(w, tStep, filter, content);
    file.endStep();
}


void
JsonExportModule :: terminate()
{
    file.close();
}
} // end namespace oofem
