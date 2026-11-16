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


JsonLinesWriter :: JsonLinesWriter(FILE *stream) : stream(stream), domainsOpen(false)
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
    if ( !isList.empty() ) {
        OOFEM_ERROR("stand-alone line can not be written within the output of a step");
    }
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
    if ( !isList.empty() ) {
        OOFEM_ERROR("output of the step already begun");
    }
    // the step object is the root level of the line; the "kind" member is its first member
    put("{\"kind\":\"step\"");
    isList.assign(1, false);
    isFirst.assign(1, false);
    domainsOpen = false;
    this->field( "step", tStep->giveNumber() );
    this->field("time", time);
    this->field( "metastep", tStep->giveMetaStepNumber() );
    this->field("solution_time", tStep->solutionTime);
}


void
JsonLinesWriter :: endStep()
{
    if ( domainsOpen ) {
        if ( isList.size() != 2 ) {
            OOFEM_ERROR("unbalanced output of the step (domain not closed)");
        }
        this->close(true);
        domainsOpen = false;
    }
    if ( isList.size() != 1 ) {
        OOFEM_ERROR("unbalanced output of the step (sections, records, lists or groups not closed)");
    }
    put("}\n");
    isList.clear();
    isFirst.clear();
}


void
JsonLinesWriter :: beginDomain(int number)
{
    // domains are items of the "domains" list of the step object, the list is opened with the first domain
    if ( !domainsOpen ) {
        if ( isList.size() != 1 ) {
            OOFEM_ERROR("output of the domain must be begun at the step level");
        }
        this->open("domains", true);
        domainsOpen = true;
    }
    if ( isList.size() != 2 ) {
        OOFEM_ERROR("output of the domain must be begun at the step level");
    }
    // key() refuses members of the domains list (reserved for domains), so the item is opened here
    if ( !isFirst.back() ) {
        put(",");
    }
    isFirst.back() = false;
    put("{");
    isList.push_back(false);
    isFirst.push_back(true);
    this->field("number", number);
}


void
JsonLinesWriter :: endDomain()
{
    if ( !domainsOpen || isList.size() != 3 ) {
        OOFEM_ERROR("endDomain without matching beginDomain");
    }
    this->close(false);
}


void
JsonLinesWriter :: beginRecord(int number, int label, const char *className)
{
    if ( isList.empty() || !isList.back() ) {
        OOFEM_ERROR("record must be reported within a section");
    }
    this->open(nullptr, false);
    // zero number or label means not applicable
    if ( number ) {
        this->field("number", number);
    }
    if ( label ) {
        this->field("label", label);
    }
    if ( className ) {
        this->field("class", className);
    }
}


bool
JsonLinesWriter :: isReservedLevel() const
{
    // the step object (level 1), the domains list (level 2) and a domain object (level 3)
    // are opened and closed only by beginStep/beginDomain and endDomain/endStep
    return isList.size() < 2 || ( domainsOpen && isList.size() <= 3 );
}


void
JsonLinesWriter :: key(const char *name)
{
    if ( isList.empty() ) {
        OOFEM_ERROR("field reported outside of the output of a step");
    }
    if ( domainsOpen && isList.size() == 2 ) {
        OOFEM_ERROR("members of the step must be reported before the first domain");
    }
    if ( !isFirst.back() ) {
        put(",");
    }
    isFirst.back() = false;
    if ( !isList.back() ) {
        put( escape(name ? name : "") );
        put(":");
    }
}


void
JsonLinesWriter :: open(const char *name, bool list)
{
    this->key(name);
    put(list ? "[" : "{");
    isList.push_back(list);
    isFirst.push_back(true);
}


void
JsonLinesWriter :: close(bool list)
{
    put(list ? "]" : "}");
    isList.pop_back();
    isFirst.pop_back();
}


void
JsonLinesWriter :: beginList(const char *name)
{
    this->open(name, true);
}


void
JsonLinesWriter :: endList()
{
    if ( this->isReservedLevel() || !isList.back() ) {
        OOFEM_ERROR("endList without matching beginList");
    }
    this->close(true);
}


void
JsonLinesWriter :: beginGroup(const char *name)
{
    this->open(name, false);
}


void
JsonLinesWriter :: endGroup()
{
    if ( this->isReservedLevel() || isList.back() ) {
        OOFEM_ERROR("endGroup (endRecord) without matching beginGroup (beginRecord)");
    }
    this->close(false);
}


void
JsonLinesWriter :: field(const char *name, bool value)
{
    key(name);
    put(value ? "true" : "false");
}


void
JsonLinesWriter :: field(const char *name, int value)
{
    key(name);
    put( std :: to_string(value) );
}


void
JsonLinesWriter :: field(const char *name, double value)
{
    key(name);
    put( number(value) );
}


void
JsonLinesWriter :: field(const char *name, const char *value)
{
    key(name);
    put( escape(value) );
}


void
JsonLinesWriter :: field(const char *name, const FloatArray &value)
{
    key(name);
    std :: string s = "[";
    for ( int i = 1; i <= value.giveSize(); ++i ) {
        if ( i > 1 ) {
            s += ',';
        }
        s += number( value.at(i) );
    }
    s += ']';
    put(s);
}


void
JsonLinesWriter :: field(const char *name, const IntArray &value)
{
    key(name);
    std :: string s = "[";
    for ( int i = 1; i <= value.giveSize(); ++i ) {
        if ( i > 1 ) {
            s += ',';
        }
        s += std :: to_string( value.at(i) );
    }
    s += ']';
    put(s);
}


void
JsonLinesWriter :: field(const char *name, const FloatMatrix &value)
{
    key(name);
    // row-wise nested arrays
    std :: string s = "[";
    for ( int i = 1; i <= value.giveNumberOfRows(); ++i ) {
        s += ( i > 1 ) ? ",[" : "[";
        for ( int j = 1; j <= value.giveNumberOfColumns(); ++j ) {
            if ( j > 1 ) {
                s += ',';
            }
            s += number( value.at(i, j) );
        }
        s += ']';
    }
    s += ']';
    put(s);
}
} // end namespace oofem
