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

#include "fei2dquadbiquad.h"
#include "floatmatrix.h"
#include "floatarray.h"
#include "floatarrayf.h"
#include "floatmatrixf.h"
#include "gaussintegrationrule.h"

namespace oofem {

FloatArrayF<9>
FEI2dQuadBiQuad :: evalN(const FloatArrayF<2> &lcoords)
{
    double u = lcoords[0];
    double v = lcoords[1];

    std::array<double, 3> a = {0.5 * ( u - 1.0 ) * u, 1.0 - u * u, 0.5 * ( u + 1.0 ) * u};
    std::array<double, 3> b = {0.5 * ( v - 1.0 ) * v, 1.0 - v * v, 0.5 * ( v + 1.0 ) * v};

    return {
        a [ 0 ] * b [ 0 ],
        a [ 2 ] * b [ 0 ],
        a [ 2 ] * b [ 2 ],
        a [ 0 ] * b [ 2 ],
        a [ 1 ] * b [ 0 ],
        a [ 2 ] * b [ 1 ],
        a [ 1 ] * b [ 2 ],
        a [ 0 ] * b [ 1 ],
        a [ 1 ] * b [ 1 ],
    };
}

void
FEI2dQuadBiQuad :: evalN(FloatArray &answer, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // evaluated by the fixed size kernel, the single implementation of the shape functions
    answer = evalN(giveFixedLocalCoordinates(lcoords));
}


FloatMatrixF<2,9>
FEI2dQuadBiQuad :: evaldNdxi(const FloatArrayF<2> &lc) 
{
    double u = lc[0];
    double v = lc[1];

    std::array<double, 3> a = {0.5 * ( u - 1.0 ) * u, 1.0 - u * u, 0.5 * ( u + 1.0 ) * u};
    std::array<double, 3> b = {0.5 * ( v - 1.0 ) * v, 1.0 - v * v, 0.5 * ( v + 1.0 ) * v};

    std::array<double, 3> da = {u - 0.5, -2.0 * u, u + 0.5};
    std::array<double, 3> db = {v - 0.5, -2.0 * v, v + 0.5};

    return {
        da [ 0 ] * b [ 0 ],
        a [ 0 ] * db [ 0 ],
        da [ 2 ] * b [ 0 ],
        a [ 2 ] * db [ 0 ],
        da [ 2 ] * b [ 2 ],
        a [ 2 ] * db [ 2 ],
        da [ 0 ] * b [ 2 ],
        a [ 0 ] * db [ 2 ],
        da [ 1 ] * b [ 0 ],
        a [ 1 ] * db [ 0 ],
        da [ 2 ] * b [ 1 ],
        a [ 2 ] * db [ 1 ],
        da [ 1 ] * b [ 2 ],
        a [ 1 ] * db [ 2 ],
        da [ 0 ] * b [ 1 ],
        a [ 0 ] * db [ 1 ],
        da [ 1 ] * b [ 1 ],
        a [ 1 ] * db [ 1 ],
    };
}


void
FEI2dQuadBiQuad :: evaldNdxi(FloatMatrix &dN, const FloatArray &lc, const FEICellGeometry &cellgeo) const
{
    // evaluated by the fixed size kernel, the single implementation of the derivatives; it returns
    // them transposed (one column per node), while this interface expects one row per node
    dN = transpose(evaldNdxi(giveFixedLocalCoordinates(lc)));
}


std::pair<double, FloatMatrixF<2,9>>
FEI2dQuadBiQuad :: _evaldNdx(const FloatArrayF<2> &lcoords, const FEICellGeometry &cellgeo) const
{
    auto dn = evaldNdxi(lcoords);
    auto jacT = giveJacobianTransposed(dn, cellgeo);
    return {det(jacT), dot(inv(jacT), dn)};
}


double
FEI2dQuadBiQuad :: evaldNdx(FloatMatrix &answer, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // A cell with a different geometry interpolation (e.g. MPM cells) is handled by the generic FEI2dQuadQuad version.
    const FEInterpolation *geoInterp = cellgeo.getGeometryInterpolation();
    if ( geoInterp && geoInterp != this ) {
        return FEI2dQuadQuad :: evaldNdx(answer, lcoords, cellgeo);
    }
    // Own fixed size kernel (the FEI2dQuadQuad one works with 8 nodes only). It returns dN/dx
    // transposed (2x9), while this interface expects one row per node (9x2).
    auto tmp = _evaldNdx(giveFixedLocalCoordinates(lcoords), cellgeo);
    answer = transpose(tmp.second);
    return tmp.first;
}


void
FEI2dQuadBiQuad :: giveJacobianMatrixAt(FloatMatrix &jacobianMatrix, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // fixed size kernel gives J^T; replaces the generic FEInterpolation2d version (FloatMatrix temporaries)
    // (9 nodes, not inherited from FEI2dQuadQuad)
    jacobianMatrix = transpose(giveJacobianTransposed(evaldNdxi(giveFixedLocalCoordinates(lcoords)), cellgeo));
}


double
FEI2dQuadBiQuad :: giveTransformationJacobian(const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // Only det(J) is needed: evaluated directly by the fixed size kernel, without the generic
    // path through giveJacobianMatrixAt (FloatMatrix temporaries). Called for every integration
    // point by computeVolumeAround.
    // 9 nodes, not inherited from FEI2dQuadQuad.
    return det(giveJacobianTransposed(evaldNdxi(giveFixedLocalCoordinates(lcoords)), cellgeo));
}


std::unique_ptr<IntegrationRule> FEI2dQuadBiQuad :: giveIntegrationRule(int order, Element_Geometry_Type egt) const
{
    auto iRule = std::make_unique<GaussIntegrationRule>(1, nullptr);
    int points = iRule->getRequiredNumberOfIntegrationPoints(_Square, order + 6);
    iRule->SetUpPointsOnSquare(points, _Unknown);
    return std::move(iRule);
}
} // end namespace oofem
