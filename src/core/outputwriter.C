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

#include "outputwriter.h"
#include "oofemcfg.h"
#include "floatarray.h"
#include "floatmatrix.h"
#include "intarray.h"
#include "timestep.h"
#include "engngm.h"
#include "error.h"

#include <cmath>
#include <cstring>
#include <ctime>

namespace oofem {
std :: string captureOutput(const std :: function< void(FILE *) > &printer)
{
    std :: string result;
    FILE *tmp = std :: tmpfile();
    if ( !tmp ) {
        OOFEM_WARNING("Can not create temporary file to capture the output");
        return result;
    }
    printer(tmp);
    fflush(tmp);
    long size = ftell(tmp);
    if ( size > 0 ) {
        result.resize(size);
        rewind(tmp);
        size_t n = fread(& result [ 0 ], 1, size, tmp);
        result.resize(n);
    }
    fclose(tmp);
    return result;
}


JsonLinesWriter :: JsonLinesWriter(FILE *stream) : stream(stream), domain(0)
{ }


std :: string
JsonLinesWriter :: escape(const char *str)
{
    std :: string out = "\"";
    for ( const char *c = str; *c; ++c ) {
        switch ( *c ) {
        case '"': out += "\\\""; break;
        case '\\': out += "\\\\"; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        case '\b': out += "\\b"; break;
        case '\f': out += "\\f"; break;
        default:
            if ( (unsigned char)*c < 0x20 ) {
                char buf [ 8 ];
                snprintf(buf, sizeof( buf ), "\\u%04x", (unsigned int)(unsigned char)*c);
                out += buf;
            } else {
                out += *c;
            }
        }
    }
    out += '"';
    return out;
}


std :: string
JsonLinesWriter :: number(double value)
{
    if ( !std :: isfinite(value) ) {
        return "null";
    }
    char buf [ 32 ];
    snprintf(buf, sizeof( buf ), "%.16e", value); // 17 significant digits: exact round trip of double
    return buf;
}


void
JsonLinesWriter :: writeLine(const char *kind, const std :: string &members)
{
    fprintf(stream, "{\"kind\":%s%s}\n", escape(kind).c_str(), members.c_str() );
}


void
JsonLinesWriter :: beginOutput(EngngModel *emodel)
{
    std :: string m;
    m += ",\"program\":\"OOFEM\"";
    m += ",\"version\":" + escape(PRG_VERSION_SHORT);
    m += ",\"git_hash\":" + escape(OOFEM_GIT_HASH);
    m += ",\"problem\":" + escape( emodel->giveClassName() );
    m += ",\"description\":" + escape( emodel->giveDescription().c_str() );
    char buf [ 64 ];
    strftime( buf, sizeof( buf ), "%Y-%m-%dT%H:%M:%S", localtime( & emodel->giveStartTime() ) );
    m += ",\"start\":" + escape(buf);
    writeLine("header", m);
    fflush(stream);
}


void
JsonLinesWriter :: endOutput(EngngModel *emodel)
{
    int rhrs, rmin, rsec, uhrs, umin, usec;
    emodel->giveAnalysisTime(rhrs, rmin, rsec, uhrs, umin, usec);
    std :: string m;
    m += ",\"real_time\":" + std :: to_string(rhrs * 3600 + rmin * 60 + rsec);
    m += ",\"user_time\":" + std :: to_string(uhrs * 3600 + umin * 60 + usec);
    writeLine("footer", m);
    fflush(stream);
}


void
JsonLinesWriter :: beginStep(TimeStep *tStep, double time)
{
    stepContext = ",\"step\":" + std :: to_string( tStep->giveNumber() ) + ",\"time\":" + number(time);
    writeLine("step", stepContext + ",\"metastep\":" + std :: to_string( tStep->giveMetaStepNumber() ) +
              ",\"solution_time\":" + number(tStep->solutionTime) );
}


void
JsonLinesWriter :: beginRecord(const char *kind, int number, int label, const char *className)
{
    buffer = "{\"kind\":" + escape(kind) + stepContext;
    if ( domain ) {
        buffer += ",\"domain\":" + std :: to_string(domain);
    }
    if ( number ) {
        buffer += ",\"number\":" + std :: to_string(number);
    }
    if ( label ) {
        buffer += ",\"label\":" + std :: to_string(label);
    }
    if ( className ) {
        buffer += ",\"class\":" + escape(className);
    }
    isList.assign(1, false);
    isFirst.assign(1, false);
}


void
JsonLinesWriter :: endRecord()
{
    if ( isList.size() != 1 ) {
        OOFEM_ERROR("unbalanced record (lists or groups not closed)");
    }
    buffer += "}\n";
    fputs(buffer.c_str(), stream);
    buffer.clear();
    isList.clear();
    isFirst.clear();
}


void
JsonLinesWriter :: key(const char *name)
{
    if ( isList.empty() ) {
        OOFEM_ERROR("field reported outside of a record");
    }
    if ( !isFirst.back() ) {
        buffer += ',';
    }
    isFirst.back() = false;
    if ( !isList.back() ) {
        buffer += escape(name ? name : "");
        buffer += ':';
    }
}


void
JsonLinesWriter :: beginList(const char *name)
{
    key(name);
    buffer += '[';
    isList.push_back(true);
    isFirst.push_back(true);
}


void
JsonLinesWriter :: endList()
{
    if ( isList.size() < 2 || !isList.back() ) {
        OOFEM_ERROR("endList without matching beginList");
    }
    buffer += ']';
    isList.pop_back();
    isFirst.pop_back();
}


void
JsonLinesWriter :: beginGroup(const char *name)
{
    key(name);
    buffer += '{';
    isList.push_back(false);
    isFirst.push_back(true);
}


void
JsonLinesWriter :: endGroup()
{
    if ( isList.size() < 2 || isList.back() ) {
        OOFEM_ERROR("endGroup without matching beginGroup");
    }
    buffer += '}';
    isList.pop_back();
    isFirst.pop_back();
}


void
JsonLinesWriter :: field(const char *name, int value)
{
    key(name);
    buffer += std :: to_string(value);
}


void
JsonLinesWriter :: field(const char *name, double value)
{
    key(name);
    buffer += number(value);
}


void
JsonLinesWriter :: field(const char *name, const char *value)
{
    key(name);
    buffer += escape(value);
}


void
JsonLinesWriter :: field(const char *name, const FloatArray &value)
{
    key(name);
    buffer += '[';
    for ( int i = 1; i <= value.giveSize(); ++i ) {
        if ( i > 1 ) {
            buffer += ',';
        }
        buffer += number( value.at(i) );
    }
    buffer += ']';
}


void
JsonLinesWriter :: field(const char *name, const IntArray &value)
{
    key(name);
    buffer += '[';
    for ( int i = 1; i <= value.giveSize(); ++i ) {
        if ( i > 1 ) {
            buffer += ',';
        }
        buffer += std :: to_string( value.at(i) );
    }
    buffer += ']';
}


void
JsonLinesWriter :: field(const char *name, const FloatMatrix &value)
{
    key(name);
    buffer += '[';
    for ( int i = 1; i <= value.giveNumberOfRows(); ++i ) {
        buffer += ( i > 1 ) ? ",[" : "[";
        for ( int j = 1; j <= value.giveNumberOfColumns(); ++j ) {
            if ( j > 1 ) {
                buffer += ',';
            }
            buffer += number( value.at(i, j) );
        }
        buffer += ']';
    }
    buffer += ']';
}
} // end namespace oofem
