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

// Unit tests of the output writers (structured output of output modules)

#include "outputwriter.h"
#include "floatarray.h"
#include "floatmatrix.h"
#include "intarray.h"

#include <catch2/catch_test_macros.hpp>
#include <cstdio>
#include <string>

using namespace oofem;

TEST_CASE("captureOutput returns the text written to the stream", "[outputwriter]")
{
    std :: string text = captureOutput([] (FILE *f) { fprintf(f, "node %d\n  dof %d d % .8e\n", 1, 2, 1.5); });
    REQUIRE(text == "node 1\n  dof 2 d  1.50000000e+00\n");
    REQUIRE(captureOutput([] (FILE *) { }).empty());
}

TEST_CASE("JsonLinesWriter escapes strings and numbers", "[outputwriter]")
{
    REQUIRE(JsonLinesWriter :: escape("a\"b\\c\nd\t") == "\"a\\\"b\\\\c\\nd\\t\"");
    REQUIRE(JsonLinesWriter :: number(1.0 / 0.0) == "null");
    REQUIRE(JsonLinesWriter :: number(-2.5) == "-2.5000000000e+00");
}

TEST_CASE("JsonLinesWriter writes one line per record", "[outputwriter]")
{
    std :: string text = captureOutput([] (FILE *f) {
        JsonLinesWriter w(f);
        w.beginDomain(1);
        w.beginRecord("element", 3, 7, "Truss1d");
        w.field("i", 2);
        w.field("s", "x");
        FloatArray a(2);
        a.at(1) = 1.;
        a.at(2) = 2.;
        w.field("a", a);
        w.field("ia", IntArray{4, 5});
        FloatMatrix m(2, 2);
        m.at(1, 1) = 1.;
        m.at(2, 2) = 2.;
        w.field("m", m);
        w.beginList("gp");
        w.beginGroup();
        w.field("number", 1);
        w.endGroup();
        w.beginGroup();
        w.field("number", 2);
        w.legacyText("t");
        w.endGroup();
        w.endList();
        w.beginGroup("g");
        w.endGroup();
        w.endRecord();
        w.endDomain();
        w.beginRecord("reaction", 0, 0);
        w.endRecord();
    });
    std :: string e = "1.0000000000e+00", z = "0.0000000000e+00";
    REQUIRE(text ==
            "{\"kind\":\"element\",\"domain\":1,\"number\":3,\"label\":7,\"class\":\"Truss1d\","
            "\"i\":2,\"s\":\"x\",\"a\":[" + e + ",2.0000000000e+00],\"ia\":[4,5],"
            "\"m\":[[" + e + "," + z + "],[" + z + ",2.0000000000e+00]],"
            "\"gp\":[{\"number\":1},{\"number\":2,\"text\":\"t\"}],\"g\":{}}\n"
            "{\"kind\":\"reaction\"}\n");
}
