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
    // Optionally, the unknown field this one is the test (weighting) function of. A deck declares
    // each unknown together with its weighting function and the two records differ only by name --
    // they may even carry different interpolations, which is what a non-symmetric
    // (Petrov-Galerkin) formulation looks like. Saying so here is what lets the assembly tell them
    // apart, which matters because nodal unknowns must be read through the unknown field's
    // interpolation rather than its weighting function's. Resolved to dualVar once all variables
    // are known, see EngngModel::instanciateMPM.
    IR_GIVE_OPTIONAL_FIELD(ir, this->dualVarName, "dualto");
}


void
Variable::postInitialize(EngngModel *problem)
{
    if ( this->dualVarName.empty() ) {
        return;
    }
    const Variable *dual = problem->giveVariableByName(this->dualVarName);
    if ( dual == this ) {
        OOFEM_ERROR("MPM variable '%s' is declared dual to itself", this->name.c_str());
    }
    // dualVar is non-const by declaration; the lookup hands out a const pointer.
    this->dualVar = const_cast< Variable * >( dual );
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
MPElement::registerStateVariable(const Variable *v)
{
    if ( v == nullptr ) {
        return;
    }

    // The state quantities that can be supplied by a primary field; keep in sync with
    // giveStateQuantityDofID.
    static const InternalStateType supported [] = {
        IST_StrainTensor, IST_DisplacementVector,
        IST_Temperature, IST_TemperatureGradient,
        IST_Pressure, IST_PressureGradient, IST_Pressure_2,
        IST_MassConcentration_1, IST_MassConcentration_2
    };

    for ( InternalStateType ist : supported ) {
        DofIDItem id = MPElement::giveStateQuantityDofID(ist);
        if ( id == Undef || !v->dofIDs.contains( (int) id ) ) {
            continue;
        }
        auto existing = this->stateVariables.find( (int) ist );
        if ( existing != this->stateVariables.end() && existing->second != v ) {
            // Two distinct unknown fields claim the same state quantity on this one cell, so there
            // is no single answer to where it comes from here. (Different cells having different
            // sources is fine and expected -- that is the multi-material case, and test functions
            // never get here.)
            OOFEM_ERROR( "on element %d the state quantity %s is supplied by more than one unknown "
                         "field ('%s' and '%s'); cannot resolve the source unambiguously",
                         this->giveNumber(), __InternalStateTypeToString(ist),
                         existing->second->name.c_str(), v->name.c_str() );
        }
        this->stateVariables [ (int) ist ] = v;
    }
}


void
MPElement::assembleStateVector(FloatArray &answer, const IntArray &istIDs, GaussPoint *gp, TimeStep *tStep)
{
    answer.clear();
    int offset = 1;

    for ( int istID : istIDs ) {
        auto it = this->stateVariables.find(istID);
        if ( it == this->stateVariables.end() ) {
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
MPElement::updateTempState(TimeStep *tStep)
{
    if ( this->stateVariables.empty() ) {
        // No term registered a primary field here, so nothing participates in the push/pull
        // protocol on this cell.
        return;
    }

    FloatArray state;

    for ( auto &iRule : this->integrationRulesArray ) {
        for ( GaussPoint *gp : *iRule ) {
            Material *mat = this->giveCrossSection()->giveMaterial(gp);
            IntArray istIDs = mat->giveStateVariableIDs( gp->giveMaterialMode() );
            if ( istIDs.isEmpty() ) {
                // material does not participate in the push/pull protocol
                continue;
            }

            // The declared layout is what the material can consume; this cell supplies whatever
            // fields act on it. If it cannot supply all of them the material is not being driven
            // in that role here -- a thermo-mechanical material used for the thermal sub-problem
            // alone has no displacement field, and a material record may sit on a cell whose terms
            // only ever ask for hardwired constants. Skip rather than push a partial state, which
            // would be positionally ambiguous and corrupt the cache.
            //
            // This is not silent: a query that does need the cached value finds it unset and says
            // so (see e.g. StructuralMaterial::giveCharacteristicVector).
            bool complete = true;
            for ( int istID : istIDs ) {
                if ( this->stateVariables.find(istID) == this->stateVariables.end() ) {
                    complete = false;
                    break;
                }
            }
            if ( !complete ) {
                continue;
            }

            this->assembleStateVector(state, istIDs, gp, tStep);
            mat->updateTempState(state, gp, tStep);
        }
    }
}


} // end namespace oofem

