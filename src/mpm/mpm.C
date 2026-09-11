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

#include "mpm.h"
#include "interpolationcatalogue.h"
#include "engngm.h"
#include "crosssection.h"
#include "material.h"
#include "internalstatetype.h"
#include "dofiditem.h"

namespace oofem {

void
Variable::initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    // read variable name
    IR_GIVE_FIELD(ir, this->name, "name");
    // read interpolation type
    std::string iname;
    IR_GIVE_FIELD(ir, iname, "interpolation");
    // get corresponding interpolation from catalogue
    this->interpolation = interpolationCatalogue.getInterpolationByName(iname);
    
    // read variable type
    ir->giveField(this->type,"type");
    // read quantity
    ir->giveField(this->q,"quantity");
    // read variable size
    IR_GIVE_FIELD(ir, this->size, "size");
    // read dofs 
    // IR_GIVE_FIELD(ir, this->dofIDs, "dofs");
    IR_GIVE_ENUM_ARRAY_FIELD(ir, this->dofIDs, "dofs", DofIDItem);
}    

void
Term::initializeFrom(const std::shared_ptr<InputRecord> &ir, EngngModel* problem)
{
    // read field and test field ids (names)
    std::string name;
    IR_GIVE_FIELD(ir, name, "variable");
    this->field = problem->giveVariableByName(name);
    IR_GIVE_FIELD(ir, name, "testvariable");
    this->testField = problem->giveVariableByName(name);
    IR_GIVE_FIELD(ir, this->mode, "mmode");
}


void
MPElement::computeGradSymMatrixAt(FloatMatrix &answer, const Variable *v, GaussPoint *gp) const
{
    const FEInterpolation *interpol = v->interpolation;
    const MaterialMode mmode = gp->giveMaterialMode();
    const int nnodes = interpol->giveNumberOfNodes( this->giveGeometryType() );
    const int ndofs = v->size;

    // matrix of shape function derivatives; entry (i,j) is dNi/dxj
    FloatMatrix dndx;
    interpol->evaldNdx( dndx, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );

    if ( ( mmode == _3dUP ) || ( mmode == _3dUPV ) || ( mmode == _3dMat ) ) {
        answer.resize(6, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
            answer(1, i * ndofs + 1) = dndx(i, 1);
            answer(2, i * ndofs + 2) = dndx(i, 2);

            answer(3, i * ndofs + 1) = dndx(i, 2);
            answer(3, i * ndofs + 2) = dndx(i, 1);

            answer(4, i * ndofs + 0) = dndx(i, 2);
            answer(4, i * ndofs + 2) = dndx(i, 0);

            answer(5, i * ndofs + 0) = dndx(i, 1);
            answer(5, i * ndofs + 1) = dndx(i, 0);
        }
    } else if ( ( mmode == _2dUP ) || ( mmode == _2dUPV ) ) {
        answer.resize(6, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
            answer(1, i * ndofs + 1) = dndx(i, 1);

            answer(5, i * ndofs + 0) = dndx(i, 1);
            answer(5, i * ndofs + 1) = dndx(i, 0);
        }
    } else if ( mmode == _PlaneStress ) {
        answer.resize(3, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
            answer(1, i * ndofs + 1) = dndx(i, 1);

            answer(2, i * ndofs + 0) = dndx(i, 1);
            answer(2, i * ndofs + 1) = dndx(i, 0);
        }
    } else if ( mmode == _PlaneStrain ) {
        answer.resize(4, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
            answer(1, i * ndofs + 1) = dndx(i, 1);

            answer(3, i * ndofs + 0) = dndx(i, 1);
            answer(3, i * ndofs + 1) = dndx(i, 0);
        }
    } else if ( mmode == _1dUP ) {
        answer.resize(6, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
        }
    } else if ( mmode == _1dMat ) {
        answer.resize(1, nnodes * ndofs);
        answer.zero();
        for ( int i = 0; i < nnodes; i++ ) {
            answer(0, i * ndofs + 0) = dndx(i, 0);
        }
    } else {
        OOFEM_ERROR( "Unsupported material mode %s", __MaterialModeToString(mmode) );
    }
}


void
MPElement::computeGradMatrixAt(FloatMatrix &answer, const Variable *v, GaussPoint *gp) const
{
    if ( v->size != 1 ) {
        OOFEM_ERROR("gradient operator expects a scalar field variable (size=1), got size %d", v->size);
    }
    const FEInterpolation *interpol = v->interpolation;
    const MaterialMode mmode = gp->giveMaterialMode();

    FloatMatrix dndx;
    interpol->evaldNdx( dndx, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
    const int nnodes = interpol->giveNumberOfNodes( this->giveGeometryType() );
    const int nsd = 1 * mmodeIs1D(mmode) + 2 * mmodeIs2D(mmode) + 3 * mmodeIs3D(mmode);

    // One row per spatial direction. Note this is a plain transpose of dndx restricted to the
    // active directions; for nsd == 1 it degenerates to the single-row form.
    answer.resize(nsd, nnodes);
    answer.zero();
    for ( int i = 0; i < nnodes; i++ ) {
        for ( int j = 0; j < nsd; j++ ) {
            answer(j, i) = dndx(i, j);
        }
    }
}


void
MPElement::computeNMatrixAt(FloatMatrix &answer, const Variable *v, GaussPoint *gp) const
{
    FloatArray nvec;
    v->interpolation->evalN( nvec, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(this) );
    answer.beNMatrixOf(nvec, v->size);
}


DofIDItem
MPElement::giveStateQuantityDofID(int istID)
{
    switch ( (InternalStateType) istID ) {
    case IST_StrainTensor:
    case IST_DisplacementVector:
        return D_u;
    case IST_Temperature:
    case IST_TemperatureGradient:
        return T_f;
    case IST_Pressure:
    case IST_PressureGradient:
        return P_f;
    case IST_Pressure_2:
        return P_f2;
    case IST_MassConcentration_1:
        return C_1;
    case IST_MassConcentration_2:
        return C_2;
    default:
        return Undef;
    }
}


void
MPElement::assembleStateVector(FloatArray &answer, const IntArray &istIDs, const StateVariableMap &vars,
                               GaussPoint *gp, TimeStep *tStep)
{
    answer.clear();
    int offset = 1;

    for ( int istID : istIDs ) {
        auto it = vars.find(istID);
        if ( it == vars.end() ) {
            OOFEM_ERROR( "no primary field supplies state quantity %s(%d) required by the material on "
                         "element %d; expected an unknown variable carrying dof id %d",
                         __InternalStateTypeToString( (InternalStateType) istID ), istID,
                         this->giveNumber(), (int) MPElement::giveStateQuantityDofID(istID) );
        }
        const Variable *v = it->second;

        FloatArray u, contribution;
        FloatMatrix op;
        this->getUnknownVector(u, v, VM_TotalIntrinsic, tStep);

        switch ( (InternalStateType) istID ) {
        case IST_StrainTensor:
            this->computeGradSymMatrixAt(op, v, gp);
            break;
        case IST_TemperatureGradient:
        case IST_PressureGradient:
            this->computeGradMatrixAt(op, v, gp);
            break;
        default:
            this->computeNMatrixAt(op, v, gp);
            break;
        }

        contribution.beProductOf(op, u);
        answer.copySubVector(contribution, offset);
        offset += contribution.giveSize();
    }
}


void
MPElement::updateTempState(const StateVariableMap &vars, TimeStep *tStep)
{
    FloatArray state;

    for ( auto &iRule : this->integrationRulesArray ) {
        for ( GaussPoint *gp : *iRule ) {
            Material *mat = this->giveCrossSection()->giveMaterial(gp);
            IntArray istIDs = mat->giveStateVariableIDs( gp->giveMaterialMode() );
            if ( istIDs.isEmpty() ) {
                continue;
            }
            this->assembleStateVector(state, istIDs, vars, gp, tStep);
            mat->updateTempState(state, gp, tStep);
        }
    }
}


} // end namespace oofem

