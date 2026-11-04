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
#include "termlibrary.h"
#include "element.h"
#include "material.h"
#include "materialmode.h"
#include "crosssection.h"

namespace oofem {

REGISTER_Term(BTamNTerm)
REGISTER_Term(NTamTBTerm)
REGISTER_Term(NTcN)




BTSigTerm::BTSigTerm (const Variable *testField, const Variable* unknownField) : Term(testField, unknownField) {}


void BTSigTerm::evaluate_lin (FloatMatrix& answer, MPElement& e, GaussPoint* gp, TimeStep* tstep) const {
    FloatMatrix D, B, DB;
    e.giveCrossSection()->giveMaterial(gp)->giveCharacteristicMatrix(D, TangentStiffness, gp, tstep);
    this->grad(B, this->field, this->field->interpolation, e, gp->giveNaturalCoordinates(), gp->giveMaterialMode());
    DB.beProductOf(D, B);
    //answer.plusProductSymmUpper(B, DB, 1.0);
    answer.beTProductOf(B,DB);
}

void BTSigTerm::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tstep) const  {
    FloatArray sig;
    FloatMatrix B;
    // The strain is no longer derived here: it was pushed to the material as part of the
    // generalized state before this sweep, so the stress is a cache read.
    this->grad(B, this->field, this->field->interpolation, cell, gp->giveNaturalCoordinates(), gp->giveMaterialMode());
    cell.giveCrossSection()->giveMaterial(gp)->giveCharacteristicVector(sig, Stress, gp, tstep);
    answer.beTProductOf(B, sig);
}

void BTSigTerm::getDimensions(Element& cell) const  {
    //int nnodes = interpol.giveNumberOfNodes();
    //int ndofs = v.size;
    //return nnodes*ndofs;
}
void BTSigTerm::initializeCell(Element& cell) const  {}

void BTSigTerm::grad(FloatMatrix& answer, const Variable *v, const FEInterpolation* interpol, const Element& cell, const FloatArray& coords, const MaterialMode mmode) const {
    FloatMatrix dndx;
    int nnodes = interpol->giveNumberOfNodes(cell.giveGeometryType());
    int ndofs = v->size;
    // evaluate matrix of derivatives, the member at i,j position contains value of dNi/dxj
    MPElement::evaldNdx(dndx, interpol, cell, coords);

    if ((mmode == _3dMat)|| (mmode == _3dUP) || (mmode == _3dUPV)) {
        // 3D mode only now
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);
            answer(2, i*ndofs+2) = dndx(i, 2);

            answer(3, i*ndofs+1) = dndx(i, 2);
            answer(3, i*ndofs+2) = dndx(i, 1);

            answer(4, i*ndofs+0) = dndx(i, 2);
            answer(4, i*ndofs+2) = dndx(i, 0);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if ((mmode == _2dUP) || (mmode == _2dUPV)) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if (mmode == _1dUP) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
        }
    }
}

//wTgNTfTerm class (H)


gNTfTerm::gNTfTerm (const Variable *testField, const Variable* unknownField, MatResponseMode lhst, MatResponseMode rtype) : Term(testField, unknownField), lhsType(lhst), rhsType(rtype) {}


void gNTfTerm::evaluate_lin (FloatMatrix& answer, MPElement& e, GaussPoint* gp, TimeStep* tstep) const  {
    FloatMatrix D, B, DB;
    e.giveCrossSection()->giveMaterial(gp)->giveCharacteristicMatrix(D, lhsType, gp, tstep); // update
    this->grad(B, this->field, this->field->interpolation, e, gp->giveNaturalCoordinates());
    DB.beProductOf(D, B);
    answer.beTProductOf(B, DB);
}

void gNTfTerm::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tstep) const  {
    FloatArray fp;
    FloatMatrix B;
    // The field gradient was pushed to the material as part of the generalized state before this
    // sweep, so the flux is a cache read.
    this->grad(B, this->field, this->field->interpolation, cell, gp->giveNaturalCoordinates());
    cell.giveCrossSection()->giveMaterial(gp)->giveCharacteristicVector(fp, rhsType, gp, tstep);
    answer.beTProductOf(B, fp);
}

void gNTfTerm::getDimensions(Element& cell) const  {
    //int nnodes = interpol.giveNumberOfNodes();
    //int ndofs = v.size;
    //return nnodes*ndofs;
}
void gNTfTerm::initializeCell(Element& cell) const  {}

void gNTfTerm::grad(FloatMatrix& answer, const Variable *v, const FEInterpolation* interpol, const Element& cell, const FloatArray& coords) const {
    FloatMatrix at;
    // evaluate matrix of derivatives, the member at i,j position contains value of dNi/dxj
    MPElement::evaldNdx(at, interpol, cell, coords);
    answer.beTranspositionOf(at);
}

// BTamN Term (Qp)

BTamNTerm::BTamNTerm (const Variable *testField, const Variable* unknownField, MatResponseMode at) : GenericCellTerm(testField, unknownField, _Unknown), aType(at) {}

void BTamNTerm::evaluate_lin (FloatMatrix& answer, MPElement& e, GaussPoint* gp, TimeStep* tstep) const  {
    FloatMatrix B, mn;
    FloatArray m=Vec6(1,1,1,0,0,0), Np;
    MaterialMode mmode = (mode==0)?gp->giveMaterialMode():mode;
    this->field->interpolation->evalN(Np, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&e));
    m.times(e.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(aType, gp, tstep));
    mn.beDyadicProductOf(m, Np);
    this->grad(B, this->testField, this->testField->interpolation, e, gp->giveNaturalCoordinates(), mmode);
    answer.beTProductOf(B, mn);
}

void BTamNTerm::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tstep) const  {
    // Q p with Q = B^T m a N^T (see evaluate_lin) is evaluated as B^T (m a (N p)): a dot product and one
    // B^T-vector product instead of forming Q
    FloatArray p, Np, m=Vec6(1,1,1,0,0,0);
    FloatMatrix B;
    MaterialMode mmode = (mode==0)?gp->giveMaterialMode():mode;
    cell.getUnknownVector(p, this->field, VM_TotalIntrinsic, tstep);
    this->field->interpolation->evalN(Np, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&cell));
    m.times(cell.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(aType, gp, tstep) * Np.dotProduct(p));
    this->grad(B, this->testField, this->testField->interpolation, cell, gp->giveNaturalCoordinates(), mmode);
    answer.beTProductOf(B, m);
}

void BTamNTerm::getDimensions(Element& cell) const  {
    //int nnodes = interpol.giveNumberOfNodes();
    //int ndofs = v.size;
    //return nnodes*ndofs;
}

void BTamNTerm::grad(FloatMatrix& answer, const Variable *v, const FEInterpolation* interpol, const Element& cell, const FloatArray& coords, const MaterialMode mmode) const {
 FloatMatrix dndx;
    int nnodes = interpol->giveNumberOfNodes(cell.giveGeometryType());
    int ndofs = v->size;
    // evaluate matrix of derivatives, the member at i,j position contains value of dNi/dxj
    MPElement::evaldNdx(dndx, interpol, cell, coords);

    if ((mmode == _3dUP) || (mmode == _3dMat)) {
        // 3D mode only now
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);
            answer(2, i*ndofs+2) = dndx(i, 2);

            answer(3, i*ndofs+1) = dndx(i, 2);
            answer(3, i*ndofs+2) = dndx(i, 1);

            answer(4, i*ndofs+0) = dndx(i, 2);
            answer(4, i*ndofs+2) = dndx(i, 0);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if ((mmode == _2dUP)||(mmode == _PlaneStress)||(mmode == _PlaneStrain)) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if (mmode == _1dUP) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
        }
    }
}

// NTamTBTerm Term (Q^T du/dt)

NTamTBTerm::NTamTBTerm (const Variable *testField, const Variable* unknownField, MatResponseMode at, ValueModeType ufieldVM) : GenericCellTerm(testField, unknownField, _Unknown), aType(at), unknownFieldVMT(ufieldVM) {}

void NTamTBTerm::evaluate_lin (FloatMatrix& answer, MPElement& e, GaussPoint* gp, TimeStep* tstep) const  {
    FloatMatrix B, mb;
    FloatArray m=Vec6(1,1,1,0,0,0), Np;
    MaterialMode mmode = (mode==0)?gp->giveMaterialMode():mode;
    this->testField->interpolation->evalN(Np, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&e));
    m.times(e.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(aType, gp, tstep));
    this->grad(B, this->field, this->field->interpolation, e, gp->giveNaturalCoordinates(), mmode);
    mb.beTProductOf(FloatMatrix::fromArray(m), B);
    FloatMatrix Npm=FloatMatrix::fromArray(Np);
    answer.beProductOf(Npm, mb);
}

void NTamTBTerm::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tstep) const  {
    // Q u with Q = N a m^T B (see evaluate_lin) is evaluated as N a (m^T (B u)): one B-vector product and
    // a dot product instead of forming Q
    FloatArray ut, eps, m=Vec6(1,1,1,0,0,0);
    FloatMatrix B;
    MaterialMode mmode = (mode==0)?gp->giveMaterialMode():mode;
    cell.getUnknownVector(ut, this->field, unknownFieldVMT ,tstep);
    this->grad(B, this->field, this->field->interpolation, cell, gp->giveNaturalCoordinates(), mmode);
    eps.beProductOf(B, ut);
    this->testField->interpolation->evalN(answer, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&cell));
    answer.times(cell.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(aType, gp, tstep) * m.dotProduct(eps));
}

void NTamTBTerm::getDimensions(Element& cell) const  {
    //int nnodes = interpol.giveNumberOfNodes();
    //int ndofs = v.size;
    //return nnodes*ndofs;
}

void NTamTBTerm::grad(FloatMatrix& answer, const Variable *v, const FEInterpolation* interpol, const Element& cell, const FloatArray& coords, const MaterialMode mmode) const {
 FloatMatrix dndx;
    int nnodes = interpol->giveNumberOfNodes(cell.giveGeometryType());
    int ndofs = v->size;
    // evaluate matrix of derivatives, the member at i,j position contains value of dNi/dxj
    MPElement::evaldNdx(dndx, interpol, cell, coords);

    if ((mmode == _3dUP) || (mmode == _3dMat)) {
        // 3D mode only now
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);
            answer(2, i*ndofs+2) = dndx(i, 2);

            answer(3, i*ndofs+1) = dndx(i, 2);
            answer(3, i*ndofs+2) = dndx(i, 1);

            answer(4, i*ndofs+0) = dndx(i, 2);
            answer(4, i*ndofs+2) = dndx(i, 0);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if ((mmode == _2dUP) || (mmode == _PlaneStress)||(mmode == _PlaneStrain)) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
            answer(1, i*ndofs+1) = dndx(i, 1);

            answer(5, i*ndofs+0) = dndx(i, 1);
            answer(5, i*ndofs+1) = dndx(i, 0);
        }
    } else if (mmode == _1dUP) {
        answer.resize(6, nnodes*ndofs);
        for (int i = 0; i< nnodes; i++) {
            answer(0, i*ndofs+0) = dndx(i, 0);
        }
    }
}


// NTcN Term (S(dp/dt))

NTcN::NTcN (const Variable *testField, const Variable* unknownField, MatResponseMode ctype, ValueModeType uFieldVMT) : GenericCellTerm(testField, unknownField, _Unknown), ctype(ctype), unknownFieldVMT(uFieldVMT) {}

void NTcN::evaluate_lin (FloatMatrix& answer, MPElement& e, GaussPoint* gp, TimeStep* tstep) const  {
    FloatArray Np;
    this->field->interpolation->evalN(Np, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&e));
    answer.beDyadicProductOf(Np, Np);
    answer.times(e.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(ctype, gp, tstep));
}

void NTcN::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tstep) const  {
    // S p with S = c N N^T (see evaluate_lin) is evaluated as N c (N p): a dot product instead of forming S
    FloatArray p;
    cell.getUnknownVector(p, this->field, unknownFieldVMT, tstep);
    this->field->interpolation->evalN(answer, gp->giveNaturalCoordinates(), FEIElementGeometryWrapper(&cell));
    answer.times(cell.giveCrossSection()->giveMaterial(gp)->giveCharacteristicValue(ctype, gp, tstep) * answer.dotProduct(p));
}

void NTcN::getDimensions(Element& cell) const  {
    //int nnodes = interpol.giveNumberOfNodes();
    //int ndofs = v.size;
    //return nnodes*ndofs;
}

// A external flux term $S=(N)^T f$, where $f$ is functor evaluating the flux. 
NTf_Surface::NTf_Surface (const Variable *testField, const NTfFunctor& f, int surf) : Term (testField, testField), f(f), isurf(surf) {}

void 
NTf_Surface::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tStep) const {
  FloatArray nvec, flux;
  FloatMatrix N;
  const FloatArray& lc = gp->giveNaturalCoordinates();

  this->f.evaluate(flux, lc, cell, this->testField, tStep);
  
  this->testField->interpolation->boundarySurfaceEvalN(nvec, isurf, lc, FEIElementGeometryWrapper(&cell));
  N.beNMatrixOf(nvec, testField->size);
  answer.beTProductOf(N, flux);
  
}

// A external flux term $S=(N)^T f$, where $f$ is functor evaluating the flux. 
NTf_Edge::NTf_Edge (const Variable *testField, const NTfFunctor& f, int surf) : Term (testField, testField), f(f), isurf(surf) {}

void 
NTf_Edge::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tStep) const {
  FloatArray nvec, flux;
  FloatMatrix N;
  const FloatArray& lc = gp->giveNaturalCoordinates();

  this->f.evaluate(flux, lc, cell, this->testField, tStep);
  
  this->testField->interpolation->boundaryEdgeEvalN(nvec, isurf, lc, FEIElementGeometryWrapper(&cell));
  N.beNMatrixOf(nvec, testField->size);
  answer.beTProductOf(N, flux);
  
}


// A external flux term $S=(N)^T f$, where $f$ is functor evaluating the flux. 
NTf_Body::NTf_Body (const Variable *testField, const NTfFunctor& f) : Term (testField, testField), f(f) {}

void 
NTf_Body::evaluate (FloatArray& answer, MPElement& cell, GaussPoint* gp, TimeStep* tStep) const {
  FloatArray nvec, flux;
  FloatMatrix N;
  const FloatArray& lc = gp->giveNaturalCoordinates();

  this->f.evaluate(flux, lc, cell, this->testField, tStep);
  
  this->testField->interpolation->evalN(nvec, lc, FEIElementGeometryWrapper(&cell));
  N.beNMatrixOf(nvec, testField->size);
  answer.beTProductOf(N, flux);
  
}


} // end namespace oofem
