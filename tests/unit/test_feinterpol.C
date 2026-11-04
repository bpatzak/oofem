/*
 * Unit tests of the geometry related services of continuum interpolations
 * (evaldNdx, giveJacobianMatrixAt, giveTransformationJacobian).
 *
 * Every interpolation is evaluated on a distorted element and compared to a reference
 * computed here by the generic algorithm from the dynamic evaldNdxi:
 *   J_ij = sum_a x_a,i dN_a/dxi_j,  dN/dx = dN/dxi J^-1,  detJ = det(J).
 * In addition, the consistency conditions sum_a dN_a/dx = 0 and sum_a x_a dN_a/dx = I are checked.
 * evaldNdxi itself is checked against central differences of evalN, so that N and dN/dxi
 * implemented separately (e.g. with different node orderings) are caught.
 */
#include "fei2dquadlin.h"
#include "fei2dquadquad.h"
#include "fei2dquadbiquad.h"
#include "fei2dtrlin.h"
#include "fei2dtrquad.h"
#include "fei3dhexalin.h"
#include "fei3dhexaquad.h"
#include "fei3dhexatriquad.h"
#include "fei3dtetlin.h"
#include "fei3dtetquad.h"
#include "fei3dwedgelin.h"
#include "fei3dwedgequad.h"
#include "floatmatrix.h"
#include "floatarray.h"

#include <catch2/catch_test_macros.hpp>
#include <catch2/matchers/catch_matchers_floating_point.hpp>
#include <vector>
#include <cmath>

using namespace oofem;
using Catch::Matchers::WithinAbs;

namespace {
const double tol = 1.e-10;

/// Smooth, orientation preserving map of parent coordinates to a distorted physical element.
Coordinates distort(double u, double v, double w)
{
    return {
        2.0 * u + 0.3 * v + 0.1 * w + 0.10 * v * v + 1.0,
        0.2 * u + 1.5 * v - 0.2 * w + 0.08 * u * w - 0.5,
        0.1 * u + 0.25 * v + 1.2 * w + 0.05 * u * v + 2.0
    };
}

std::vector< Coordinates > distortNodes(const std::vector< std::array< double, 3 > > &ref)
{
    std::vector< Coordinates > x;
    for ( auto &r : ref ) {
        x.push_back(distort(r [ 0 ], r [ 1 ], r [ 2 ]) );
    }
    return x;
}

/**
 * Checks the geometry services of interpolation at given parent coordinates.
 * @param comps Global coordinate components used by the interpolation (xind, yind[, zind]).
 * @param detSign Sign of the determinant returned by evaldNdx/giveTransformationJacobian relative to det(dx/dxi)
 * (-1 for FEI3dTetLin, whose closed form follows the opposite node orientation).
 * @param checkJacobianMatrix Whether giveJacobianMatrixAt is provided.
 */
void checkGeometry(const FEInterpolation &interp, const std::vector< Coordinates > &nodes, Element_Geometry_Type egt,
                   const FloatArray &lcoords, const std::vector< int > &comps, double detSign = 1., bool checkJacobianMatrix = true)
{
    FEIVertexListGeometryWrapper cellgeo(nodes, egt);
    int nsd = ( int ) comps.size();
    int nn = ( int ) nodes.size();

    // reference
    FloatMatrix dNdxi, jac(nsd, nsd), jacInv, dNdxRef;
    interp.evaldNdxi(dNdxi, lcoords, cellgeo);
    REQUIRE(dNdxi.giveNumberOfRows() == nn);
    REQUIRE(dNdxi.giveNumberOfColumns() == nsd);
    for ( int a = 1; a <= nn; a++ ) {
        for ( int i = 1; i <= nsd; i++ ) {
            for ( int j = 1; j <= nsd; j++ ) {
                jac.at(i, j) += nodes [ a - 1 ].at(comps [ i - 1 ]) * dNdxi.at(a, j);
            }
        }
    }
    double detRef = jac.giveDeterminant();
    REQUIRE(detSign * detRef > 1.e-3);
    jacInv.beInverseOf(jac);
    dNdxRef.beProductOf(dNdxi, jacInv);

    // the reference relies on evaldNdxi: check it against central differences of evalN
    // (independent of how evaldNdxi is implemented) and the partition of unity
    FloatArray n, np, nm;
    interp.evalN(n, lcoords, cellgeo);
    REQUIRE(n.giveSize() == nn);
    CHECK_THAT(n.sum(), WithinAbs(1., tol) );
    const double h = 1.e-5;
    for ( int j = 1; j <= nsd; j++ ) {
        FloatArray lp = lcoords, lm = lcoords;
        lp.at(j) += h;
        lm.at(j) -= h;
        interp.evalN(np, lp, cellgeo);
        interp.evalN(nm, lm, cellgeo);
        double sum = 0.;
        for ( int a = 1; a <= nn; a++ ) {
            CHECK_THAT(dNdxi.at(a, j), WithinAbs( ( np.at(a) - nm.at(a) ) / ( 2. * h ), 1.e-7) );
            sum += dNdxi.at(a, j);
        }
        CHECK_THAT(sum, WithinAbs(0., tol) );
    }

    // evaldNdx
    FloatMatrix dNdx;
    double det = interp.evaldNdx(dNdx, lcoords, cellgeo);
    CHECK_THAT(det, WithinAbs(detSign * detRef, tol * std::fabs(detRef) ) );
    REQUIRE(dNdx.giveNumberOfRows() == nn);
    REQUIRE(dNdx.giveNumberOfColumns() == nsd);
    for ( int a = 1; a <= nn; a++ ) {
        for ( int k = 1; k <= nsd; k++ ) {
            CHECK_THAT(dNdx.at(a, k), WithinAbs(dNdxRef.at(a, k), tol) );
        }
    }

    // consistency: sum_a dN_a/dx = 0, sum_a x_a dN_a/dx = I
    for ( int k = 1; k <= nsd; k++ ) {
        double sum = 0.;
        for ( int a = 1; a <= nn; a++ ) {
            sum += dNdx.at(a, k);
        }
        CHECK_THAT(sum, WithinAbs(0., tol) );
        for ( int i = 1; i <= nsd; i++ ) {
            double xdN = 0.;
            for ( int a = 1; a <= nn; a++ ) {
                xdN += nodes [ a - 1 ].at(comps [ i - 1 ]) * dNdx.at(a, k);
            }
            CHECK_THAT(xdN, WithinAbs(i == k ? 1. : 0., tol) );
        }
    }

    // giveTransformationJacobian
    CHECK_THAT(interp.giveTransformationJacobian(lcoords, cellgeo), WithinAbs(detSign * detRef, tol * std::fabs(detRef) ) );

    // giveJacobianMatrixAt
    if ( !checkJacobianMatrix ) {
        return;
    }
    FloatMatrix jacobian;
    interp.giveJacobianMatrixAt(jacobian, lcoords, cellgeo);
    REQUIRE(jacobian.giveNumberOfRows() == nsd);
    REQUIRE(jacobian.giveNumberOfColumns() == nsd);
    for ( int i = 1; i <= nsd; i++ ) {
        for ( int j = 1; j <= nsd; j++ ) {
            CHECK_THAT(jacobian.at(i, j), WithinAbs(jac.at(i, j), tol) );
        }
    }
}

void check3d(const FEInterpolation &interp, const std::vector< std::array< double, 3 > > &ref, Element_Geometry_Type egt,
             const std::vector< FloatArray > &points, double detSign = 1., bool checkJacobianMatrix = true)
{
    auto nodes = distortNodes(ref);
    for ( auto &lc : points ) {
        checkGeometry(interp, nodes, egt, lc, { 1, 2, 3 }, detSign, checkJacobianMatrix);
    }
}

void check2d(const FEInterpolation &interp, const std::vector< std::array< double, 3 > > &ref, Element_Geometry_Type egt,
             const std::vector< FloatArray > &points)
{
    auto nodes = distortNodes(ref);
    for ( auto &lc : points ) {
        checkGeometry(interp, nodes, egt, lc, { 1, 2 });
    }
}

const std::vector< FloatArray > cubePoints = { Vec3(0.1, -0.3, 0.45), Vec3(-0.577, 0.577, -0.577), Vec3(0.9, -0.8, 0.2) };
const std::vector< FloatArray > tetPoints = { Vec3(0.1, 0.2, 0.3), Vec3(0.25, 0.25, 0.25), Vec3(0.6, 0.1, 0.05) };
const std::vector< FloatArray > wedgePoints = { Vec3(0.2, 0.3, -0.4), Vec3(1. / 3., 1. / 3., 0.), Vec3(0.6, 0.1, 0.9) };
const std::vector< FloatArray > squarePoints = { Vec2(0.3, -0.6), Vec2(-0.577, 0.577), Vec2(0.9, 0.1) };
const std::vector< FloatArray > trianglePoints = { Vec2(0.2, 0.3), Vec2(1. / 3., 1. / 3.), Vec2(0.7, 0.1) };

const std::vector< std::array< double, 3 > > hexa20 = {
    { -1, -1, 1 }, { -1, 1, 1 }, { 1, 1, 1 }, { 1, -1, 1 },
    { -1, -1, -1 }, { -1, 1, -1 }, { 1, 1, -1 }, { 1, -1, -1 },
    { -1, 0, 1 }, { 0, 1, 1 }, { 1, 0, 1 }, { 0, -1, 1 },
    { -1, 0, -1 }, { 0, 1, -1 }, { 1, 0, -1 }, { 0, -1, -1 },
    { -1, -1, 0 }, { -1, 1, 0 }, { 1, 1, 0 }, { 1, -1, 0 }
};

const std::vector< std::array< double, 3 > > tet10 = {
    { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, 0 },
    { .5, .5, 0 }, { 0, .5, .5 }, { .5, 0, .5 }, { .5, 0, 0 }, { 0, .5, 0 }, { 0, 0, .5 }
};
} // end anonymous namespace


TEST_CASE("tests/unit/feinterpol/3d", "[FEInterpolation]")
{
    SECTION("FEI3dHexaLin") {
        std::vector< std::array< double, 3 > > hexa8(hexa20.begin(), hexa20.begin() + 8);
        check3d(FEI3dHexaLin(), hexa8, EGT_hexa_1, cubePoints);
    }
    SECTION("FEI3dHexaQuad") {
        check3d(FEI3dHexaQuad(), hexa20, EGT_hexa_2, cubePoints);
    }
    SECTION("FEI3dHexaTriQuad") {
        auto hexa27 = hexa20;
        hexa27.insert(hexa27.end(), {
            { 0, 0, 1 }, { 0, 0, -1 }, { -1, 0, 0 }, { 0, 1, 0 }, { 1, 0, 0 }, { 0, -1, 0 }, { 0, 0, 0 }
        });
        check3d(FEI3dHexaTriQuad(), hexa27, EGT_hexa_27, cubePoints);
    }
    SECTION("FEI3dTetLin") {
        // tet4 node orientation of OOFEM (nodes 2 and 3 of tet10 swapped): its closed form detJ is -det(dx/dxi),
        // positive for valid elements; no giveJacobianMatrixAt (see FEI3dTetLin::evaldNdxi)
        std::vector< std::array< double, 3 > > tet4 = { { 1, 0, 0 }, { 0, 0, 1 }, { 0, 1, 0 }, { 0, 0, 0 } };
        check3d(FEI3dTetLin(), tet4, EGT_tetra_1, tetPoints, -1., false);
    }
    SECTION("FEI3dTetQuad") {
        check3d(FEI3dTetQuad(), tet10, EGT_tetra_2, tetPoints);
    }
    SECTION("FEI3dWedgeLin") {
        check3d(FEI3dWedgeLin(), { { 0, 0, -1 }, { 1, 0, -1 }, { 0, 1, -1 }, { 0, 0, 1 }, { 1, 0, 1 }, { 0, 1, 1 } },
                EGT_wedge_1, wedgePoints);
    }
    SECTION("FEI3dWedgeQuad") {
        FEI3dWedgeQuad interp;
        FloatMatrix lnodes;
        interp.giveLocalNodeCoords(lnodes, EGT_wedge_2);
        std::vector< std::array< double, 3 > > wedge15;
        for ( int i = 1; i <= lnodes.giveNumberOfColumns(); i++ ) {
            wedge15.push_back({ lnodes.at(1, i), lnodes.at(2, i), lnodes.at(3, i) });
        }
        check3d(interp, wedge15, EGT_wedge_2, wedgePoints);
    }
}


TEST_CASE("tests/unit/feinterpol/2d", "[FEInterpolation]")
{
    SECTION("FEI2dQuadLin") {
        check2d(FEI2dQuadLin(1, 2), { { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 } }, EGT_quad_1, squarePoints);
    }
    SECTION("FEI2dQuadQuad") {
        check2d(FEI2dQuadQuad(1, 2), {
            { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 0, -1, 0 }, { 1, 0, 0 }
        }, EGT_quad_2, squarePoints);
    }
    SECTION("FEI2dQuadBiQuad") {
        check2d(FEI2dQuadBiQuad(1, 2), {
            { -1, -1, 0 }, { 1, -1, 0 }, { 1, 1, 0 }, { -1, 1, 0 }, { 0, -1, 0 }, { 1, 0, 0 }, { 0, 1, 0 }, { -1, 0, 0 }, { 0, 0, 0 }
        }, EGT_quad9_2, squarePoints);
    }
    SECTION("FEI2dTrLin") {
        check2d(FEI2dTrLin(1, 2), { { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 } }, EGT_triangle_1, trianglePoints);
    }
    SECTION("FEI2dTrQuad") {
        check2d(FEI2dTrQuad(1, 2), {
            { 1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 0 }, { .5, .5, 0 }, { 0, .5, 0 }, { .5, 0, 0 }
        }, EGT_triangle_2, trianglePoints);
    }
}


TEST_CASE("tests/unit/feinterpol/axisymmetric", "[FEInterpolation]")
{
    // giveTransformationJacobian of axisymmetric interpolations includes the radius r = sum_a N_a x_a
    auto nodes = distortNodes({ { 1, 1, 0 }, { -1, 1, 0 }, { -1, -1, 0 }, { 1, -1, 0 } });
    FEIVertexListGeometryWrapper cellgeo(nodes, EGT_quad_1);
    FEI2dQuadLin plane(1, 2);
    FEI2dQuadLinAxi axi(1, 2);
    for ( auto &lc : squarePoints ) {
        FloatArray n;
        axi.evalN(n, lc, cellgeo);
        double r = 0.;
        for ( int a = 1; a <= 4; a++ ) {
            r += n.at(a) * nodes [ a - 1 ].at(1);
        }
        double detJ = plane.giveTransformationJacobian(lc, cellgeo);
        CHECK_THAT(axi.giveTransformationJacobian(lc, cellgeo), WithinAbs(r * detJ, tol * std::fabs(r * detJ) ) );
    }
}
