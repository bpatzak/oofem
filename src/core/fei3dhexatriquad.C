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

#include "fei3dhexatriquad.h"
#include "intarray.h"
#include "floatarray.h"
#include "floatarrayf.h"
#include "floatmatrix.h"
#include "floatmatrixf.h"
#include "gaussintegrationrule.h"

namespace oofem {

FloatArrayF<27>
FEI3dHexaTriQuad :: evalN(const FloatArrayF<3> &lcoords)
{
    //auto [u, v, w] = lcoords;
    double u = lcoords[0];
    double v = lcoords[1];
    double w = lcoords[2];

    std::array<double, 3> a = {0.5 * ( u - 1.0 ) * u, 0.5 * ( u + 1.0 ) * u, 1.0 - u * u};
    std::array<double, 3> b = {0.5 * ( v - 1.0 ) * v, 0.5 * ( v + 1.0 ) * v, 1.0 - v * v};
    std::array<double, 3> c = {0.5 * ( w - 1.0 ) * w, 0.5 * ( w + 1.0 ) * w, 1.0 - w * w};

    return {
        a [ 0 ] * b [ 0 ] * c [ 1 ],
        a [ 0 ] * b [ 1 ] * c [ 1 ],
        a [ 1 ] * b [ 1 ] * c [ 1 ],
        a [ 1 ] * b [ 0 ] * c [ 1 ],
        a [ 0 ] * b [ 0 ] * c [ 0 ],
        a [ 0 ] * b [ 1 ] * c [ 0 ],
        a [ 1 ] * b [ 1 ] * c [ 0 ],
        a [ 1 ] * b [ 0 ] * c [ 0 ],
        a [ 0 ] * b [ 2 ] * c [ 1 ],
        a [ 2 ] * b [ 1 ] * c [ 1 ],
        a [ 1 ] * b [ 2 ] * c [ 1 ],
        a [ 2 ] * b [ 0 ] * c [ 1 ],
        a [ 0 ] * b [ 2 ] * c [ 0 ],
        a [ 2 ] * b [ 1 ] * c [ 0 ],
        a [ 1 ] * b [ 2 ] * c [ 0 ],
        a [ 2 ] * b [ 0 ] * c [ 0 ],
        a [ 0 ] * b [ 0 ] * c [ 2 ],
        a [ 0 ] * b [ 1 ] * c [ 2 ],
        a [ 1 ] * b [ 1 ] * c [ 2 ],
        a [ 1 ] * b [ 0 ] * c [ 2 ],
        a [ 2 ] * b [ 2 ] * c [ 1 ],
        a [ 2 ] * b [ 2 ] * c [ 0 ],
        a [ 0 ] * b [ 2 ] * c [ 2 ],
        a [ 2 ] * b [ 1 ] * c [ 2 ],
        a [ 1 ] * b [ 2 ] * c [ 2 ],
        a [ 2 ] * b [ 0 ] * c [ 2 ],
        a [ 2 ] * b [ 2 ] * c [ 2 ]
    };
}

void
FEI3dHexaTriQuad :: evalN(FloatArray &answer, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // evaluated by the fixed size kernel, the single implementation of the shape functions
    answer = evalN(giveFixedLocalCoordinates(lcoords));
}


void
FEI3dHexaTriQuad :: surfaceEvalN(FloatArray &answer, int isurf, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    double u = lcoords.at(1);
    double v = lcoords.at(2);

    double a[] = {
        0.5 * ( u - 1. ) * u, 0.5 * ( u + 1. ) * u, 1. - u * u
    };
    double b[] = {
        0.5 * ( v - 1. ) * v, 0.5 * ( v + 1. ) * v, 1. - v * v
    };

    answer.resize(9);

    answer.at(1) = a [ 0 ] * b [ 0 ];
    answer.at(2) = a [ 1 ] * b [ 0 ];
    answer.at(5) = a [ 2 ] * b [ 0 ];

    answer.at(4) = a [ 0 ] * b [ 1 ];
    answer.at(3) = a [ 1 ] * b [ 1 ];
    answer.at(7) = a [ 2 ] * b [ 1 ];

    answer.at(8) = a [ 0 ] * b [ 2 ];
    answer.at(6) = a [ 1 ] * b [ 2 ];
    answer.at(9) = a [ 2 ] * b [ 2 ];
}


double
FEI3dHexaTriQuad :: surfaceEvalNormal(FloatArray &answer, int isurf, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    FloatArray e1, e2, dNdu(9), dNdv(9);

    double u = lcoords.at(1);
    double v = lcoords.at(2);

    double a[] = {
        0.5 * ( u - 1. ) * u, 0.5 * ( u + 1. ) * u, 1. - u * u
    };
    double b[] = {
        0.5 * ( v - 1. ) * v, 0.5 * ( v + 1. ) * v, 1. - v * v
    };

    double da[] = {
        u - 0.5, u + 0.5, -2. * u
    };
    double db[] = {
        v - 0.5, v + 0.5, -2. * v
    };

    dNdu.at(1) = da [ 0 ] * b [ 0 ];
    dNdu.at(2) = da [ 1 ] * b [ 0 ];
    dNdu.at(5) = da [ 2 ] * b [ 0 ];

    dNdu.at(4) = da [ 0 ] * b [ 1 ];
    dNdu.at(3) = da [ 1 ] * b [ 1 ];
    dNdu.at(7) = da [ 2 ] * b [ 1 ];

    dNdu.at(8) = da [ 0 ] * b [ 2 ];
    dNdu.at(6) = da [ 1 ] * b [ 2 ];
    dNdu.at(9) = da [ 2 ] * b [ 2 ];


    dNdv.at(1) = a [ 0 ] * db [ 0 ];
    dNdv.at(2) = a [ 1 ] * db [ 0 ];
    dNdv.at(5) = a [ 2 ] * db [ 0 ];

    dNdv.at(4) = a [ 0 ] * db [ 1 ];
    dNdv.at(3) = a [ 1 ] * db [ 1 ];
    dNdv.at(7) = a [ 2 ] * db [ 1 ];

    dNdv.at(8) = a [ 0 ] * db [ 2 ];
    dNdv.at(6) = a [ 1 ] * db [ 2 ];
    dNdv.at(9) = a [ 2 ] * db [ 2 ];

    const auto &snodes = this->computeLocalSurfaceMapping(isurf);
    for ( int i = 1; i <= 9; ++i ) {
        e1.add( dNdu.at(i), cellgeo.giveVertexCoordinates( snodes.at(i) ) );
        e2.add( dNdv.at(i), cellgeo.giveVertexCoordinates( snodes.at(i) ) );
    }

    answer.beVectorProductOf(e1, e2);
    return answer.normalize_giveNorm();
}


IntArray
FEI3dHexaTriQuad :: computeLocalSurfaceMapping(int isurf) const
{
    if ( isurf == 1 ) {
        return { 2, 1, 4, 3,  9, 12, 11, 10, 21};
    } else if ( isurf == 2 ) {
        return { 5, 6, 7, 8, 13, 14, 15, 16, 22};
    } else if ( isurf == 3 ) {
        return { 1, 2, 6, 5,  9, 18, 13, 17, 23};
    } else if ( isurf == 4 ) {
        return { 2, 3, 7, 6, 10, 19, 14, 18, 24};
    } else if ( isurf == 5 ) {
        return { 3, 4, 8, 7, 11, 20, 15, 19, 25};
    } else if ( isurf == 6 ) {
        return { 4, 1, 5, 8, 12, 17, 16, 20, 26};
    } else {
        throw std::range_error("invalid surface number");
    }
}


FloatMatrixF<3,27>
FEI3dHexaTriQuad :: evaldNdxi(const FloatArrayF<3> &lcoords) 
{
    //auto [u, v, w] = lcoords;
    double u = lcoords[0];
    double v = lcoords[1];
    double w = lcoords[2];

    std::array<double, 3> a = {0.5 * ( u - 1.0 ) * u, 0.5 * ( u + 1.0 ) * u, 1.0 - u * u};
    std::array<double, 3> b = {0.5 * ( v - 1.0 ) * v, 0.5 * ( v + 1.0 ) * v, 1.0 - v * v};
    std::array<double, 3> c = {0.5 * ( w - 1.0 ) * w, 0.5 * ( w + 1.0 ) * w, 1.0 - w * w};

    std::array<double, 3> da = {-0.5 + u, 0.5 + u, -2.0 * u};
    std::array<double, 3> db = {-0.5 + v, 0.5 + v, -2.0 * v};
    std::array<double, 3> dc = {-0.5 + w, 0.5 + w, -2.0 * w};

    return {
        da [ 0 ] * b [ 0 ] * c [ 1 ], a [ 0 ] * db [ 0 ] * c [ 1 ], a [ 0 ] * b [ 0 ] * dc [ 1 ],
        da [ 0 ] * b [ 1 ] * c [ 1 ], a [ 0 ] * db [ 1 ] * c [ 1 ], a [ 0 ] * b [ 1 ] * dc [ 1 ],
        da [ 1 ] * b [ 1 ] * c [ 1 ], a [ 1 ] * db [ 1 ] * c [ 1 ], a [ 1 ] * b [ 1 ] * dc [ 1 ],
        da [ 1 ] * b [ 0 ] * c [ 1 ], a [ 1 ] * db [ 0 ] * c [ 1 ], a [ 1 ] * b [ 0 ] * dc [ 1 ],
        da [ 0 ] * b [ 0 ] * c [ 0 ], a [ 0 ] * db [ 0 ] * c [ 0 ], a [ 0 ] * b [ 0 ] * dc [ 0 ],
        da [ 0 ] * b [ 1 ] * c [ 0 ], a [ 0 ] * db [ 1 ] * c [ 0 ], a [ 0 ] * b [ 1 ] * dc [ 0 ],
        da [ 1 ] * b [ 1 ] * c [ 0 ], a [ 1 ] * db [ 1 ] * c [ 0 ], a [ 1 ] * b [ 1 ] * dc [ 0 ],
        da [ 1 ] * b [ 0 ] * c [ 0 ], a [ 1 ] * db [ 0 ] * c [ 0 ], a [ 1 ] * b [ 0 ] * dc [ 0 ],
        da [ 0 ] * b [ 2 ] * c [ 1 ], a [ 0 ] * db [ 2 ] * c [ 1 ], a [ 0 ] * b [ 2 ] * dc [ 1 ],
        da [ 2 ] * b [ 1 ] * c [ 1 ], a [ 2 ] * db [ 1 ] * c [ 1 ], a [ 2 ] * b [ 1 ] * dc [ 1 ],
        da [ 1 ] * b [ 2 ] * c [ 1 ], a [ 1 ] * db [ 2 ] * c [ 1 ], a [ 1 ] * b [ 2 ] * dc [ 1 ],
        da [ 2 ] * b [ 0 ] * c [ 1 ], a [ 2 ] * db [ 0 ] * c [ 1 ], a [ 2 ] * b [ 0 ] * dc [ 1 ],
        da [ 0 ] * b [ 2 ] * c [ 0 ], a [ 0 ] * db [ 2 ] * c [ 0 ], a [ 0 ] * b [ 2 ] * dc [ 0 ],
        da [ 2 ] * b [ 1 ] * c [ 0 ], a [ 2 ] * db [ 1 ] * c [ 0 ], a [ 2 ] * b [ 1 ] * dc [ 0 ],
        da [ 1 ] * b [ 2 ] * c [ 0 ], a [ 1 ] * db [ 2 ] * c [ 0 ], a [ 1 ] * b [ 2 ] * dc [ 0 ],
        da [ 2 ] * b [ 0 ] * c [ 0 ], a [ 2 ] * db [ 0 ] * c [ 0 ], a [ 2 ] * b [ 0 ] * dc [ 0 ],
        da [ 0 ] * b [ 0 ] * c [ 2 ], a [ 0 ] * db [ 0 ] * c [ 2 ], a [ 0 ] * b [ 0 ] * dc [ 2 ],
        da [ 0 ] * b [ 1 ] * c [ 2 ], a [ 0 ] * db [ 1 ] * c [ 2 ], a [ 0 ] * b [ 1 ] * dc [ 2 ],
        da [ 1 ] * b [ 1 ] * c [ 2 ], a [ 1 ] * db [ 1 ] * c [ 2 ], a [ 1 ] * b [ 1 ] * dc [ 2 ],
        da [ 1 ] * b [ 0 ] * c [ 2 ], a [ 1 ] * db [ 0 ] * c [ 2 ], a [ 1 ] * b [ 0 ] * dc [ 2 ],
        da [ 2 ] * b [ 2 ] * c [ 1 ], a [ 2 ] * db [ 2 ] * c [ 1 ], a [ 2 ] * b [ 2 ] * dc [ 1 ],
        da [ 2 ] * b [ 2 ] * c [ 0 ], a [ 2 ] * db [ 2 ] * c [ 0 ], a [ 2 ] * b [ 2 ] * dc [ 0 ],
        da [ 0 ] * b [ 2 ] * c [ 2 ], a [ 0 ] * db [ 2 ] * c [ 2 ], a [ 0 ] * b [ 2 ] * dc [ 2 ],
        da [ 2 ] * b [ 1 ] * c [ 2 ], a [ 2 ] * db [ 1 ] * c [ 2 ], a [ 2 ] * b [ 1 ] * dc [ 2 ],
        da [ 1 ] * b [ 2 ] * c [ 2 ], a [ 1 ] * db [ 2 ] * c [ 2 ], a [ 1 ] * b [ 2 ] * dc [ 2 ],
        da [ 2 ] * b [ 0 ] * c [ 2 ], a [ 2 ] * db [ 0 ] * c [ 2 ], a [ 2 ] * b [ 0 ] * dc [ 2 ],
        da [ 2 ] * b [ 2 ] * c [ 2 ], a [ 2 ] * db [ 2 ] * c [ 2 ], a [ 2 ] * b [ 2 ] * dc [ 2 ],
    };
}


void
FEI3dHexaTriQuad :: evaldNdxi(FloatMatrix &dN, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // evaluated by the fixed size kernel, the single implementation of the derivatives; it returns
    // them transposed (one column per node), while this interface expects one row per node
    dN = transpose(evaldNdxi(giveFixedLocalCoordinates(lcoords)));
}


std::pair<double, FloatMatrixF<3,27>>
FEI3dHexaTriQuad :: evaldNdx(const FloatArrayF<3> &lcoords, const FEICellGeometry &cellgeo)
{
    auto dNduvw = evaldNdxi(lcoords);
    auto jacT = giveJacobianTransposed(dNduvw, cellgeo);
    return {det(jacT), dot(inv(jacT), dNduvw)};
}

double
FEI3dHexaTriQuad :: evaldNdx(FloatMatrix &answer, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // The fixed size kernel returns dN/dx transposed (3x27), this interface expects one row per node (27x3).
    auto tmp = evaldNdx(giveFixedLocalCoordinates(lcoords), cellgeo);
    answer = transpose(tmp.second);
    return tmp.first;
}

void
FEI3dHexaTriQuad :: giveJacobianMatrixAt(FloatMatrix &jacobianMatrix, const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // Own fixed size version, the one inherited from FEI3dHexaQuad works with 20 nodes only.
    jacobianMatrix = transpose(giveJacobianTransposed(evaldNdxi(giveFixedLocalCoordinates(lcoords)), cellgeo));
}

double
FEI3dHexaTriQuad :: giveTransformationJacobian(const FloatArray &lcoords, const FEICellGeometry &cellgeo) const
{
    // Only det(J) is needed, no FloatMatrix temporaries (see FEI3dHexaLin::giveTransformationJacobian).
    return det(giveJacobianTransposed(evaldNdxi(giveFixedLocalCoordinates(lcoords)), cellgeo));
}

double
FEI3dHexaTriQuad :: evalNXIntegral(int iSurf, const FEICellGeometry &cellgeo) const
{
    const auto &fNodes = this->computeLocalSurfaceMapping(iSurf);

    const auto &c1 = cellgeo.giveVertexCoordinates( fNodes.at(1) );
    const auto &c2 = cellgeo.giveVertexCoordinates( fNodes.at(2) );
    const auto &c3 = cellgeo.giveVertexCoordinates( fNodes.at(3) );
    const auto &c4 = cellgeo.giveVertexCoordinates( fNodes.at(4) );
    const auto &c5 = cellgeo.giveVertexCoordinates( fNodes.at(5) );
    const auto &c6 = cellgeo.giveVertexCoordinates( fNodes.at(6) );
    const auto &c7 = cellgeo.giveVertexCoordinates( fNodes.at(7) );
    const auto &c8 = cellgeo.giveVertexCoordinates( fNodes.at(8) );
    const auto &c9 = cellgeo.giveVertexCoordinates( fNodes.at(9) );

    // Generated with Mathematica (rather unwieldy expression, tried to simplify it as good as possible, but it could probably be better)
    return (
               c1[2] * (
                   c2[1] * ( -3 * c3[0] - 3 * c4[0] + 18 * c6[0] - 4 * c7[0] + 18 * c8[0] + 24 * c9[0] ) +
                   c3[1] * ( 3 * c2[0] - 3 * c4[0] + 2 * c5[0] + 2 * c6[0] - 2 * c7[0] - 2 * c8[0] ) +
                   c4[1] * ( 3 * c2[0] + 3 * c3[0] - 18 * c5[0] + 4 * c6[0] - 18 * c7[0] - 24 * c9[0] ) +
                   c5[1] * ( -2 * c3[0] + 18 * c4[0] + 12 * c6[0] + 24 * c7[0] - 108 * c8[0] - 144 * c9[0] ) +
                   c6[1] * ( -18 * c2[0] - 2 * c3[0] - 4 * c4[0] - 12 * c5[0] - 4 * c7[0] + 24 * c8[0] + 16 * c9[0] ) +
                   c7[1] * ( 4 * c2[0] + 2 * c3[0] + 18 * c4[0] - 24 * c5[0] + 4 * c6[0] + 12 * c8[0] - 16 * c9[0] ) +
                   c8[1] * ( -18 * c2[0] + 2 * c3[0] + 108 * c5[0] - 24 * c6[0] - 12 * c7[0] + 144 * c9[0] ) +
                   c9[1] * ( -24 * c2[0] + 24 * c4[0] + 144 * c5[0] - 16 * c6[0] + 16 * c7[0] - 144 * c8[0] )
                   ) +
               c2[2] * (
                   c1[1] * ( 3 * c3[0] + 3 * c4[0] - 18 * c6[0] + 4 * c7[0] - 18 * c8[0] - 24 * c9[0] ) +
                   c3[1] * ( -3 * c1[0] - 3 * c4[0] + 18 * c5[0] + 18 * c7[0] - 4 * c8[0] + 24 * c9[0] ) +
                   c4[1] * ( -3 * c1[0] + 3 * c3[0] - 2 * c5[0] + 2 * c6[0] + 2 * c7[0] - 2 * c8[0] ) +
                   c5[1] * ( -18 * c3[0] + 2 * c4[0] + 108 * c6[0] - 24 * c7[0] - 12 * c8[0] + 144 * c9[0] ) +
                   c6[1] * ( 18 * c1[0] - 2 * c4[0] - 108 * c5[0] + 12 * c7[0] + 24 * c8[0] - 144 * c9[0] ) +
                   c7[1] * ( -4 * c1[0] - 18 * c3[0] - 2 * c4[0] + 24 * c5[0] - 12 * c6[0] - 4 * c8[0] + 16 * c9[0] ) +
                   c8[1] * ( 18 * c1[0] + 4 * c3[0] + 2 * c4[0] + 12 * c5[0] - 24 * c6[0] + 4 * c7[0] - 16 * c9[0] ) +
                   c9[1] * ( 24 * c1[0] - 24 * c3[0] - 144 * c5[0] + 144 * c6[0] - 16 * c7[0] + 16 * c8[0] )
                   ) +
               c3[2] * (
                   c1[1] * ( -3 * c2[0] + 3 * c4[0] - 2 * c5[0] - 2 * c6[0] + 2 * c7[0] + 2 * c8[0] ) +
                   c2[1] * ( 3 * c1[0] + 3 * c4[0] - 18 * c5[0] - 18 * c7[0] + 4 * c8[0] - 24 * c9[0] ) +
                   c4[1] * ( -3 * c1[0] - 3 * c2[0] - 4 * c5[0] + 18 * c6[0] + 18 * c8[0] + 24 * c9[0] ) +
                   c5[1] * ( 2 * c1[0] + 18 * c2[0] + 4 * c4[0] + 12 * c6[0] - 24 * c7[0] + 4 * c8[0] - 16 * c9[0] ) +
                   c6[1] * ( 2 * c1[0] - 18 * c4[0] - 12 * c5[0] + 108 * c7[0] - 24 * c8[0] + 144 * c9[0] ) +
                   c7[1] * ( -2 * c1[0] + 18 * c2[0] + 24 * c5[0] - 108 * c6[0] + 12 * c8[0] - 144 * c9[0] ) +
                   c8[1] * ( -2 * c1[0] - 4 * c2[0] - 18 * c4[0] - 4 * c5[0] + 24 * c6[0] - 12 * c7[0] + 16 * c9[0] ) +
                   c9[1] * ( 24 * c2[0] - 24 * c4[0] + 16 * c5[0] - 144 * c6[0] + 144 * c7[0] - 16 * c8[0] )
                   ) +
               c4[2] * (
                   c1[1] * ( -3 * c2[0] - 3 * c3[0] + 18 * c5[0] - 4 * c6[0] + 18 * c7[0] + 24 * c9[0] ) +
                   c2[1] * ( 3 * c1[0] - 3 * c3[0] + 2 * c5[0] - 2 * c6[0] - 2 * c7[0] + 2 * c8[0] ) +
                   c3[1] * ( 3 * c1[0] + 3 * c2[0] + 4 * c5[0] - 18 * c6[0] - 18 * c8[0] - 24 * c9[0] ) +
                   c5[1] * ( -18 * c1[0] - 2 * c2[0] - 4 * c3[0] - 4 * c6[0] + 24 * c7[0] - 12 * c8[0] + 16 * c9[0] ) +
                   c6[1] * ( 4 * c1[0] + 2 * c2[0] + 18 * c3[0] + 4 * c5[0] + 12 * c7[0] - 24 * c8[0] - 16 * c9[0] ) +
                   c7[1] * ( -18 * c1[0] + 2 * c2[0] - 24 * c5[0] - 12 * c6[0] + 108 * c8[0] + 144 * c9[0] ) +
                   c8[1] * ( -2 * c2[0] + 18 * c3[0] + 12 * c5[0] + 24 * c6[0] - 108 * c7[0] - 144 * c9[0] ) +
                   c9[1] * ( -24 * c1[0] + 24 * c3[0] - 16 * c5[0] + 16 * c6[0] - 144 * c7[0] + 144 * c8[0] )
                   ) +
               c5[2] * (
                   c1[1] * ( 2 * c3[0] - 18 * c4[0] - 12 * c6[0] - 24 * c7[0] + 108 * c8[0] + 144 * c9[0] ) +
                   c2[1] * ( 18 * c3[0] - 2 * c4[0] - 108 * c6[0] + 24 * c7[0] + 12 * c8[0] - 144 * c9[0] ) +
                   c3[1] * ( -2 * c1[0] - 18 * c2[0] - 4 * c4[0] - 12 * c6[0] + 24 * c7[0] - 4 * c8[0] + 16 * c9[0] ) +
                   c4[1] * ( 18 * c1[0] + 2 * c2[0] + 4 * c3[0] + 4 * c6[0] - 24 * c7[0] + 12 * c8[0] - 16 * c9[0] ) +
                   c6[1] * ( 12 * c1[0] + 108 * c2[0] + 12 * c3[0] - 4 * c4[0] + 32 * c7[0] + 32 * c8[0] - 192 * c9[0] ) +
                   c7[1] * ( 24 * c1[0] - 24 * c2[0] - 24 * c3[0] + 24 * c4[0] - 32 * c6[0] + 32 * c8[0] ) +
                   c8[1] * ( -108 * c1[0] - 12 * c2[0] + 4 * c3[0] - 12 * c4[0] - 32 * c6[0] - 32 * c7[0] + 192 * c9[0] ) +
                   c9[1] * ( -144 * c1[0] + 144 * c2[0] - 16 * c3[0] + 16 * c4[0] + 192 * c6[0] - 192 * c8[0] )
                   ) +
               c6[2] * (
                   c1[1] * ( 18 * c2[0] + 2 * c3[0] + 4 * c4[0] + 12 * c5[0] + 4 * c7[0] - 24 * c8[0] - 16 * c9[0] ) +
                   c2[1] * ( -18 * c1[0] + 2 * c4[0] + 108 * c5[0] - 12 * c7[0] - 24 * c8[0] + 144 * c9[0] ) +
                   c3[1] * ( -2 * c1[0] + 18 * c4[0] + 12 * c5[0] - 108 * c7[0] + 24 * c8[0] - 144 * c9[0] ) +
                   c4[1] * ( -4 * c1[0] - 2 * c2[0] - 18 * c3[0] - 4 * c5[0] - 12 * c7[0] + 24 * c8[0] + 16 * c9[0] ) +
                   c5[1] * ( -12 * c1[0] - 108 * c2[0] - 12 * c3[0] + 4 * c4[0] - 32 * c7[0] - 32 * c8[0] + 192 * c9[0] ) +
                   c8[1] * ( 24 * c1[0] + 24 * c2[0] - 24 * c3[0] - 24 * c4[0] + 32 * c5[0] - 32 * c7[0] ) +
                   c7[1] * ( -4 * c1[0] + 12 * c2[0] + 108 * c3[0] + 12 * c4[0] + 32 * c5[0] + 32 * c8[0] - 192 * c9[0] ) +
                   c9[1] * ( 16 * c1[0] - 144 * c2[0] + 144 * c3[0] - 16 * c4[0] - 192 * c5[0] + 192 * c7[0] )
                   ) +
               c7[2] * (
                   c1[1] * ( -4 * c2[0] - 2 * c3[0] - 18 * c4[0] + 24 * c5[0] - 4 * c6[0] - 12 * c8[0] + 16 * c9[0] ) +
                   c2[1] * ( 4 * c1[0] + 18 * c3[0] + 2 * c4[0] - 24 * c5[0] + 12 * c6[0] + 4 * c8[0] - 16 * c9[0] ) +
                   c3[1] * ( 2 * c1[0] - 18 * c2[0] - 24 * c5[0] + 108 * c6[0] - 12 * c8[0] + 144 * c9[0] ) +
                   c4[1] * ( 18 * c1[0] - 2 * c2[0] + 24 * c5[0] + 12 * c6[0] - 108 * c8[0] - 144 * c9[0] ) +
                   c5[1] * ( -24 * c1[0] + 24 * c2[0] + 24 * c3[0] - 24 * c4[0] + 32 * c6[0] - 32 * c8[0] ) +
                   c6[1] * ( 4 * c1[0] - 12 * c2[0] - 108 * c3[0] - 12 * c4[0] - 32 * c5[0] - 32 * c8[0] + 192 * c9[0] ) +
                   c8[1] * ( 12 * c1[0] - 4 * c2[0] + 12 * c3[0] + 108 * c4[0] + 32 * c5[0] + 32 * c6[0] - 192 * c9[0] ) +
                   c9[1] * ( -16 * c1[0] + 16 * c2[0] - 144 * c3[0] + 144 * c4[0] - 192 * c6[0] + 192 * c8[0] )
                   ) +
               c8[2] * (
                   c1[1] * ( 18 * c2[0] - 2 * c3[0] - 108 * c5[0] + 24 * c6[0] + 12 * c7[0] - 144 * c9[0] ) +
                   c2[1] * ( -18 * c1[0] - 4 * c3[0] - 2 * c4[0] - 12 * c5[0] + 24 * c6[0] - 4 * c7[0] + 16 * c9[0] ) +
                   c3[1] * ( 2 * c1[0] + 4 * c2[0] + 18 * c4[0] + 4 * c5[0] - 24 * c6[0] + 12 * c7[0] - 16 * c9[0] ) +
                   c4[1] * ( 2 * c2[0] - 18 * c3[0] - 12 * c5[0] - 24 * c6[0] + 108 * c7[0] + 144 * c9[0] ) +
                   c5[1] * ( 108 * c1[0] + 12 * c2[0] - 4 * c3[0] + 12 * c4[0] + 32 * c6[0] + 32 * c7[0] - 192 * c9[0] ) +
                   c6[1] * ( -24 * c1[0] - 24 * c2[0] + 24 * c3[0] + 24 * c4[0] - 32 * c5[0] + 32 * c7[0] ) +
                   c7[1] * ( -12 * c1[0] + 4 * c2[0] - 12 * c3[0] - 108 * c4[0] - 32 * c5[0] - 32 * c6[0] + 192 * c9[0] ) +
                   c9[1] * ( 144 * c1[0] - 16 * c2[0] + 16 * c3[0] - 144 * c4[0] + 192 * c5[0] - 192 * c7[0] )
                   ) +
               c9[2] * (
                   c1[1] * ( 24 * c2[0] - 24 * c4[0] - 144 * c5[0] + 16 * c6[0] - 16 * c7[0] + 144 * c8[0] ) +
                   c2[1] * ( -24 * c1[0] + 24 * c3[0] + 144 * c5[0] - 144 * c6[0] + 16 * c7[0] - 16 * c8[0] ) +
                   c3[1] * ( -24 * c2[0] + 24 * c4[0] - 16 * c5[0] + 144 * c6[0] - 144 * c7[0] + 16 * c8[0] ) +
                   c4[1] * ( 24 * c1[0] - 24 * c3[0] + 16 * c5[0] - 16 * c6[0] + 144 * c7[0] - 144 * c8[0] ) +
                   c5[1] * ( 144 * c1[0] - 144 * c2[0] + 16 * c3[0] - 16 * c4[0] - 192 * c6[0] + 192 * c8[0] ) +
                   c6[1] * ( -16 * c1[0] + 144 * c2[0] - 144 * c3[0] + 16 * c4[0] + 192 * c5[0] - 192 * c7[0] ) +
                   c7[1] * ( 16 * c1[0] - 16 * c2[0] + 144 * c3[0] - 144 * c4[0] + 192 * c6[0] - 192 * c8[0] ) +
                   c8[1] * ( -144 * c1[0] + 16 * c2[0] - 16 * c3[0] + 144 * c4[0] - 192 * c5[0] + 192 * c7[0] ) )
               ) / 300.0;
}

std::unique_ptr<IntegrationRule>
FEI3dHexaTriQuad :: giveIntegrationRule(int order, Element_Geometry_Type egt) const
{
    auto iRule = std::make_unique<GaussIntegrationRule>(1, nullptr);
    ///@todo Verify: Is +15 correct for dealing with "detJ"? If it is, perhaps we shouldn't go for exact integration since it is likely overkill.
    int points = iRule->getRequiredNumberOfIntegrationPoints(_Cube, order + 15);
    iRule->SetUpPointsOnCube(points, _Unknown);
    return std::move(iRule);
}

std::unique_ptr<IntegrationRule>
FEI3dHexaTriQuad :: giveBoundaryIntegrationRule(int order, int boundary, Element_Geometry_Type egt) const
{
    auto iRule = std::make_unique<GaussIntegrationRule>(1, nullptr);
    ///@todo Verify: Is +6 correct for dealing with "detJ" on this surface?
    int points = iRule->getRequiredNumberOfIntegrationPoints(_Square, order + 6);
    iRule->SetUpPointsOnSquare(points, _Unknown);
    return std::move(iRule);
}
} // end namespace oofem
