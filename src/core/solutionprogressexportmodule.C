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

#include "solutionprogressexportmodule.h"
#include "engngm.h"
#include "domain.h"
#include "dofmanager.h"
#include "dof.h"
#include "dofiditem.h"
#include "element.h"
#include "gausspoint.h"
#include "integrationrule.h"
#include "set.h"
#include "timestep.h"
#include "classfactory.h"
#include "oofemcfg.h"
#include "logger.h"
#include "error.h"

#ifdef __SM_MODULE
 #include "sm/EngineeringModels/structengngmodel.h"
#endif

#include <algorithm>
#include <cmath>
#include <ctime>
#include <limits>
#include <set>

#ifdef _WIN32
 #ifndef WIN32_LEAN_AND_MEAN
  #define WIN32_LEAN_AND_MEAN
 #endif
 #ifndef NOMINMAX
  #define NOMINMAX
 #endif
 #include <windows.h>
 #include <process.h>
#else
 #include <unistd.h>
#endif

namespace oofem {
REGISTER_ExportModule(SolutionProgressExportModule)

/// Name and version of the protocol written by this module. Bump the version on incompatible changes.
static const char *PROGRESS_PROTOCOL_NAME = "oofem-progress";
static const int PROGRESS_PROTOCOL_VERSION = 1;

namespace {
/// Returns s as a JSON string literal (quoted and escaped).
std::string jsonString(const std::string &s)
{
    std::string answer = "\"";
    for ( unsigned char c : s ) {
        switch ( c ) {
        case '"': answer += "\\\""; break;
        case '\\': answer += "\\\\"; break;
        case '\n': answer += "\\n"; break;
        case '\r': answer += "\\r"; break;
        case '\t': answer += "\\t"; break;
        default:
            if ( c < 0x20 ) {
                char buf [ 8 ];
                std::snprintf(buf, sizeof( buf ), "\\u%04x", c);
                answer += buf;
            } else {
                answer += static_cast< char >( c );
            }
        }
    }
    return answer + "\"";
}

/// Returns v as a JSON number; non-finite values (not representable in JSON) become null.
std::string jsonNumber(double v)
{
    if ( !std::isfinite(v) ) {
        return "null";
    }
    char buf [ 32 ];
    std::snprintf(buf, sizeof( buf ), "%.12g", v);
    return buf;
}

std::string jsonNumber(int v)
{
    return std::to_string(v);
}

/// Returns JSON array of numbers.
std::string jsonArray(const FloatArray &a)
{
    std::string answer = "[";
    for ( int i = 1; i <= a.giveSize(); i++ ) {
        answer += ( i > 1 ? "," : "" ) + jsonNumber( a.at(i) );
    }
    return answer + "]";
}

std::string convergedReasonToString(ConvergedReason r)
{
    switch ( r ) {
    case CR_CONVERGED: return "converged";
    case CR_DIVERGED_ITS: return "diverged_its";
    case CR_DIVERGED_TOL: return "diverged_tol";
    case CR_FAILED: return "failed";
    default: return "unknown";
    }
}

const char *valueModeToString(ValueModeType mode)
{
    switch ( mode ) {
    case VM_Velocity: return "v";
    case VM_Acceleration: return "a";
    default: return "d";
    }
}

const char *aggregateOpToString(SolutionProgressExportModule::AggregateOp op)
{
    switch ( op ) {
    case SolutionProgressExportModule::AO_Min: return "min";
    case SolutionProgressExportModule::AO_Average: return "avg";
    default: return "max";
    }
}

/// Internal state type name without the "IST_" prefix (e.g. "StressTensor").
std::string istName(InternalStateType ist)
{
    std::string name = __InternalStateTypeToString(ist);
    if ( name.compare(0, 4, "IST_") == 0 ) {
        name = name.substr(4);
    }
    return name;
}

/// Name of the machine the solver runs on (used by the viewer to check if the process is alive).
std::string giveHostName()
{
#ifdef _WIN32
    char buf [ MAX_COMPUTERNAME_LENGTH + 1 ];
    DWORD size = sizeof( buf );
    if ( GetComputerNameA(buf, & size) ) {
        return std::string(buf, size);
    }
#else
    char buf [ 256 ];
    if ( gethostname(buf, sizeof( buf ) ) == 0 ) {
        buf [ sizeof( buf ) - 1 ] = '\0';
        return buf;
    }
#endif
    return "";
}

int giveProcessId()
{
#ifdef _WIN32
    return _getpid();
#else
    return (int)getpid();
#endif
}

/// Current local time in ISO 8601 format.
std::string giveIsoTime()
{
    std::time_t now = std::time(nullptr);
    char buf [ 32 ];
    std::strftime(buf, sizeof( buf ), "%Y-%m-%dT%H:%M:%S", std::localtime(& now) );
    return buf;
}
} // end anonymous namespace


SolutionProgressExportModule :: SolutionProgressExportModule(int n, EngngModel *e) : ExportModule(n, e)
{ }


SolutionProgressExportModule :: ~SolutionProgressExportModule()
{
    if ( this->stream ) {
        fclose(this->stream);
    }
}


void
SolutionProgressExportModule :: initializeFrom(const std::shared_ptr<InputRecord> &ir)
{
    ExportModule :: initializeFrom(ir);

    this->filename = this->emodel->giveOutputBaseFileName() + ".progress.jsonl";
    IR_GIVE_OPTIONAL_FIELD(ir, this->filename, _IFT_SolutionProgressExportModule_filename);
    this->iterationsFlag = ir->hasField(_IFT_SolutionProgressExportModule_iterations);

    this->channels.clear();
    // Channels are created in this order: dofman, reactions, load level, IP values, aggregates.
    // The order defines the order of values in step records and the indices used by plot_x/plot_y.

    // DOF manager unknowns
    IntArray ids, dofs, modes;
    IR_GIVE_OPTIONAL_FIELD(ir, ids, _IFT_SolutionProgressExportModule_dofmanIds);
    if ( ids.giveSize() ) {
        IR_GIVE_FIELD(ir, dofs, _IFT_SolutionProgressExportModule_dofmanDofs);
        if ( dofs.giveSize() != ids.giveSize() ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_dofmanDofs, "size differs from dofman_ids");
        }
        modes.resize( ids.giveSize() );
        modes.zero();
        IR_GIVE_OPTIONAL_FIELD(ir, modes, _IFT_SolutionProgressExportModule_dofmanModes);
        if ( modes.giveSize() != ids.giveSize() ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_dofmanModes, "size differs from dofman_ids");
        }
        for ( int i = 1; i <= ids.giveSize(); i++ ) {
            Channel ch;
            ch.kind = CK_DofMan;
            ch.number = ids.at(i);
            ch.dofId = dofs.at(i);
            switch ( modes.at(i) ) {
            case 0: ch.mode = VM_Total; break;
            case 1: ch.mode = VM_Velocity; break;
            case 2: ch.mode = VM_Acceleration; break;
            default:
                throw ValueInputException(ir, _IFT_SolutionProgressExportModule_dofmanModes, "must be 0 (total), 1 (velocity) or 2 (acceleration)");
            }
            const char *prefix = ch.mode == VM_Velocity ? "v" : ( ch.mode == VM_Acceleration ? "a" : "u" );
            ch.id = prefix + std::to_string(ch.number) + "_" + DofIDItemToString( ( DofIDItem ) ch.dofId );
            this->channels.push_back(ch);
        }
    }

    // Sums of reactions
    IntArray rdofs, rsets;
    IR_GIVE_OPTIONAL_FIELD(ir, rdofs, _IFT_SolutionProgressExportModule_reactionDofs);
    if ( rdofs.giveSize() ) {
        rsets.resize( rdofs.giveSize() );
        rsets.zero();
        IR_GIVE_OPTIONAL_FIELD(ir, rsets, _IFT_SolutionProgressExportModule_reactionSets);
        if ( rsets.giveSize() != rdofs.giveSize() ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_reactionSets, "size differs from reaction_dofs");
        }
        for ( int i = 1; i <= rdofs.giveSize(); i++ ) {
            Channel ch;
            ch.kind = CK_Reaction;
            ch.number = rsets.at(i);
            ch.dofId = rdofs.at(i);
            ch.id = "R" + ( ch.number ? std::to_string(ch.number) : std::string() ) + "_" + DofIDItemToString( ( DofIDItem ) ch.dofId );
            this->channels.push_back(ch);
        }
    }

    // Load level
    if ( ir->hasField(_IFT_SolutionProgressExportModule_loadLevel) ) {
        Channel ch;
        ch.kind = CK_LoadLevel;
        ch.id = "lambda";
        this->channels.push_back(ch);
    }

    // Values at individual integration points
    IntArray elems, rules, gps, ists, comps;
    IR_GIVE_OPTIONAL_FIELD(ir, elems, _IFT_SolutionProgressExportModule_ipElems);
    if ( elems.giveSize() ) {
        int n = elems.giveSize();
        IR_GIVE_FIELD(ir, gps, _IFT_SolutionProgressExportModule_ipGps);
        IR_GIVE_FIELD(ir, ists, _IFT_SolutionProgressExportModule_ipIsts);
        rules.resize(n);
        rules.add(1);
        comps.resize(n);
        comps.add(1);
        IR_GIVE_OPTIONAL_FIELD(ir, rules, _IFT_SolutionProgressExportModule_ipRules);
        IR_GIVE_OPTIONAL_FIELD(ir, comps, _IFT_SolutionProgressExportModule_ipComps);
        if ( gps.giveSize() != n || ists.giveSize() != n || rules.giveSize() != n || comps.giveSize() != n ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_ipElems, "ip_gps, ip_ists, ip_rules and ip_comps must have the same size as ip_elems");
        }
        for ( int i = 1; i <= n; i++ ) {
            Channel ch;
            ch.kind = CK_IP;
            ch.number = elems.at(i);
            ch.irule = rules.at(i);
            ch.gp = gps.at(i);
            ch.ist = ( InternalStateType ) ists.at(i);
            ch.comp = comps.at(i);
            ch.id = istName(ch.ist) + "[" + std::to_string(ch.comp) + "]@e" + std::to_string(ch.number) +
                    ( ch.irule != 1 ? ".r" + std::to_string(ch.irule) : std::string() ) + ".gp" + std::to_string(ch.gp);
            this->channels.push_back(ch);
        }
    }

    // Aggregates over element sets
    IntArray asets, aists, acomps, aops;
    IR_GIVE_OPTIONAL_FIELD(ir, asets, _IFT_SolutionProgressExportModule_aggSets);
    if ( asets.giveSize() ) {
        int n = asets.giveSize();
        IR_GIVE_FIELD(ir, aists, _IFT_SolutionProgressExportModule_aggIsts);
        acomps.resize(n);
        acomps.add(1);
        aops.resize(n);
        aops.zero();
        IR_GIVE_OPTIONAL_FIELD(ir, acomps, _IFT_SolutionProgressExportModule_aggComps);
        IR_GIVE_OPTIONAL_FIELD(ir, aops, _IFT_SolutionProgressExportModule_aggOps);
        if ( aists.giveSize() != n || acomps.giveSize() != n || aops.giveSize() != n ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_aggSets, "agg_ists, agg_comps and agg_ops must have the same size as agg_sets");
        }
        for ( int i = 1; i <= n; i++ ) {
            Channel ch;
            ch.kind = CK_Aggregate;
            ch.number = asets.at(i);
            ch.ist = ( InternalStateType ) aists.at(i);
            ch.comp = acomps.at(i);
            if ( aops.at(i) < AO_Max || aops.at(i) > AO_Average ) {
                throw ValueInputException(ir, _IFT_SolutionProgressExportModule_aggOps, "must be 0 (max), 1 (min) or 2 (volume average)");
            }
            ch.op = ( AggregateOp ) aops.at(i);
            ch.id = std::string( aggregateOpToString(ch.op) ) + "(" + istName(ch.ist) + "[" + std::to_string(ch.comp) + "])@set" + std::to_string(ch.number);
            this->channels.push_back(ch);
        }
    }

    // User labels override the generated ones (in channel order)
    std::vector< std::string > labels;
    IR_GIVE_OPTIONAL_FIELD(ir, labels, _IFT_SolutionProgressExportModule_labels);
    if ( labels.size() > this->channels.size() ) {
        throw ValueInputException(ir, _IFT_SolutionProgressExportModule_labels, "more labels than channels");
    }
    for ( size_t i = 0; i < labels.size(); i++ ) {
        this->channels [ i ].id = labels [ i ];
    }

    // Make ids unique, the viewer references channels by id
    std::set< std::string > used = { "time", "step" };
    for ( auto &ch : this->channels ) {
        std::string id = ch.id;
        for ( int k = 2; used.count(id); k++ ) {
            id = ch.id + "#" + std::to_string(k);
        }
        ch.id = id;
        used.insert(id);
    }

    // Suggested default charts
    this->plotX.clear();
    this->plotY.clear();
    IR_GIVE_OPTIONAL_FIELD(ir, this->plotX, _IFT_SolutionProgressExportModule_plotX);
    IR_GIVE_OPTIONAL_FIELD(ir, this->plotY, _IFT_SolutionProgressExportModule_plotY);
    if ( this->plotX.giveSize() != this->plotY.giveSize() ) {
        throw ValueInputException(ir, _IFT_SolutionProgressExportModule_plotY, "size differs from plot_x");
    }
    for ( int i = 1; i <= this->plotX.giveSize(); i++ ) {
        int nch = (int)this->channels.size();
        if ( this->plotX.at(i) < 0 || this->plotX.at(i) > nch || this->plotY.at(i) < 0 || this->plotY.at(i) > nch ) {
            throw ValueInputException(ir, _IFT_SolutionProgressExportModule_plotX, "channel index out of range (0 = time, 1..number of channels)");
        }
    }
}


bool
SolutionProgressExportModule :: isWriter() const
{
    return !this->emodel->isParallel() || this->emodel->giveRank() == 0;
}


double
SolutionProgressExportModule :: giveElapsedTime() const
{
    return std::chrono::duration< double >( std::chrono::steady_clock::now() - this->startTime ).count();
}


void
SolutionProgressExportModule :: writeRecord(const std::string &record)
{
    if ( !this->stream ) {
        return;
    }
    // A record is always written as a single complete line followed by flush,
    // so that the reader never sees a partially written record except the last line.
    std::fputs( ( record + "\n" ).c_str(), this->stream );
    std::fflush(this->stream);
}


std::string
SolutionProgressExportModule :: giveHeaderRecord()
{
    std::string r = "{\"type\":\"header\"";
    r += ",\"protocol\":" + jsonString(PROGRESS_PROTOCOL_NAME);
    r += ",\"version\":" + jsonNumber(PROGRESS_PROTOCOL_VERSION);
    r += ",\"oofem\":" + jsonString(PRG_VERSION);
    r += ",\"git\":" + jsonString(OOFEM_GIT_HASH);
    r += ",\"host\":" + jsonString( giveHostName() );
    r += ",\"pid\":" + jsonNumber( giveProcessId() );
    r += ",\"job\":" + jsonString( this->emodel->giveOutputBaseFileName() );
    r += ",\"input\":" + jsonString( this->emodel->giveReferenceFileName() );
    r += ",\"description\":" + jsonString( this->emodel->giveDescription() );
    r += ",\"engngmodel\":" + jsonString( this->emodel->giveClassName() );
    r += ",\"nsteps\":" + jsonNumber( this->emodel->giveNumberOfSteps() );
    r += ",\"final_time\":" + jsonNumber( this->emodel->giveFinalTime() * this->timeScale );
    r += ",\"start\":" + jsonString( giveIsoTime() );
    r += ",\"iterations\":" + std::string( this->iterationsFlag ? "true" : "false" );

    r += ",\"channels\":[";
    for ( size_t i = 0; i < this->channels.size(); i++ ) {
        const Channel &ch = this->channels [ i ];
        r += ( i ? "," : "" );
        r += "{\"id\":" + jsonString(ch.id);
        switch ( ch.kind ) {
        case CK_DofMan:
            r += ",\"kind\":\"dofman\",\"dofman\":" + jsonNumber(ch.number) +
                 ",\"dof\":" + jsonString( DofIDItemToString( ( DofIDItem ) ch.dofId ) ) +
                 ",\"mode\":" + jsonString( valueModeToString(ch.mode) );
            break;
        case CK_Reaction:
            r += ",\"kind\":\"reaction\",\"set\":" + jsonNumber(ch.number) +
                 ",\"dof\":" + jsonString( DofIDItemToString( ( DofIDItem ) ch.dofId ) );
            break;
        case CK_LoadLevel:
            r += ",\"kind\":\"loadlevel\"";
            break;
        case CK_IP:
            r += ",\"kind\":\"ip\",\"element\":" + jsonNumber(ch.number) + ",\"irule\":" + jsonNumber(ch.irule) +
                 ",\"gp\":" + jsonNumber(ch.gp) + ",\"ist\":" + jsonString( istName(ch.ist) ) + ",\"comp\":" + jsonNumber(ch.comp);
            break;
        case CK_Aggregate:
            r += ",\"kind\":\"agg\",\"set\":" + jsonNumber(ch.number) + ",\"op\":" + jsonString( aggregateOpToString(ch.op) ) +
                 ",\"ist\":" + jsonString( istName(ch.ist) ) + ",\"comp\":" + jsonNumber(ch.comp);
            break;
        }
        r += "}";
    }
    r += "]";

    r += ",\"plots\":[";
    for ( int i = 1; i <= this->plotX.giveSize(); i++ ) {
        auto chId = [ this ](int index) { return index == 0 ? std::string("time") : this->channels [ index - 1 ].id; };
        r += ( i > 1 ? "," : "" );
        r += "{\"x\":" + jsonString( chId( this->plotX.at(i) ) ) + ",\"y\":" + jsonString( chId( this->plotY.at(i) ) ) + "}";
    }
    r += "]";

    // reserved for steering of the solver by the viewer (not implemented yet)
    r += ",\"control\":null";
    r += "}";
    return r;
}


void
SolutionProgressExportModule :: initialize()
{
    ExportModule :: initialize();
    this->startTime = std::chrono::steady_clock::now();

    if ( !this->isWriter() ) {
        return;
    }
    if ( this->stream ) {
        fclose(this->stream);
    }
    // The file is truncated, a reader following it detects the new run by the new header.
    this->stream = std::fopen(this->filename.c_str(), "w");
    if ( !this->stream ) {
        OOFEM_WARNING("Cannot open progress file \"%s\", progress export disabled", this->filename.c_str() );
        return;
    }

#ifndef __SM_MODULE
    for ( const auto &ch : this->channels ) {
        if ( ch.kind == CK_Reaction ) {
            OOFEM_WARNING("Reaction channels require the SM module, values will be null");
            break;
        }
    }
#endif

    this->writeRecord( this->giveHeaderRecord() );
}


void
SolutionProgressExportModule :: doOutput(TimeStep *tStep, bool forcedOutput)
{
    if ( !( testTimeStepOutput(tStep) || forcedOutput ) ) {
        return;
    }

    std::vector< double > values;
    // channels have to be evaluated by all processes (parallel evaluation may require communication)
    this->evaluateChannels(values, tStep);
    if ( !this->stream ) {
        return;
    }

    std::string r = "{\"type\":\"step\"";
    r += ",\"step\":" + jsonNumber( tStep->giveNumber() );
    r += ",\"mstep\":" + jsonNumber( tStep->giveMetaStepNumber() );
    r += ",\"time\":" + jsonNumber( tStep->giveTargetTime() * this->timeScale );
    r += ",\"dt\":" + jsonNumber( tStep->giveTimeIncrement() * this->timeScale );
    r += ",\"wall\":" + jsonNumber( this->giveElapsedTime() );
    // solver status of the last (successful or not) attempt to solve the step, as set by the engineering model
    r += ",\"solver\":{\"status\":" + jsonString( convergedReasonToString(tStep->convergedReason) ) +
         ",\"iters\":" + jsonNumber(tStep->numberOfIterations) +
         ",\"attempts\":" + jsonNumber(tStep->numberOfAttempts) +
         ",\"time\":" + jsonNumber(tStep->solutionTime) + "}";
    r += ",\"values\":[";
    for ( size_t i = 0; i < values.size(); i++ ) {
        r += ( i ? "," : "" ) + jsonNumber(values [ i ]);
    }
    r += "]}";
    this->writeRecord(r);
}


void
SolutionProgressExportModule :: doIterationOutput(TimeStep *tStep, const SolverIterationInfo &info)
{
    if ( !this->iterationsFlag || !this->stream || !testTimeStepOutput(tStep) ) {
        return;
    }

    std::string r = "{\"type\":\"iter\"";
    r += ",\"step\":" + jsonNumber( tStep->giveNumber() );
    // the attempt number is set by the engineering model before each (re)solution of the step
    r += ",\"attempt\":" + jsonNumber( std::max(tStep->numberOfAttempts, 1) );
    r += ",\"iter\":" + jsonNumber(info.iteration);
    r += ",\"status\":" + jsonString( convergedReasonToString(info.status) );
    r += ",\"groups\":[";
    if ( info.dofGroups.giveSize() == 0 ) {
        r += "\"all\"";
    }
    for ( int i = 1; i <= info.dofGroups.giveSize(); i++ ) {
        r += ( i > 1 ? "," : "" ) + jsonString( DofIDItemToString( ( DofIDItem ) info.dofGroups.at(i) ) );
    }
    r += "]";
    r += ",\"force_err\":" + jsonArray(info.forceErr);
    r += ",\"disp_err\":" + jsonArray(info.dispErr);
    r += ",\"rtolf\":" + jsonNumber(info.rtolf);
    r += ",\"rtold\":" + jsonNumber(info.rtold);
    r += ",\"wall\":" + jsonNumber( this->giveElapsedTime() );
    r += "}";
    this->writeRecord(r);
}


void
SolutionProgressExportModule :: terminate()
{
    if ( !this->stream ) {
        return;
    }
    int nwarnings = 0, nerrors = 0;
    oofem_logger.getNumberOfWarningsAndErrors(nwarnings, nerrors);

    std::string r = "{\"type\":\"end\",\"status\":\"finished\"";
    r += ",\"wall\":" + jsonNumber( this->giveElapsedTime() );
    r += ",\"warnings\":" + jsonNumber(nwarnings);
    r += ",\"errors\":" + jsonNumber(nerrors);
    r += "}";
    this->writeRecord(r);

    fclose(this->stream);
    this->stream = nullptr;
}


void
SolutionProgressExportModule :: evaluateChannels(std::vector< double > &answer, TimeStep *tStep)
{
    const double nan = std::numeric_limits< double >::quiet_NaN();
    Domain *d = this->emodel->giveDomain(1);
    answer.assign(this->channels.size(), nan);

    bool hasReactions = false;
    for ( size_t i = 0; i < this->channels.size(); i++ ) {
        const Channel &ch = this->channels [ i ];
        double value = nan;
        switch ( ch.kind ) {
        case CK_DofMan:
            if ( this->evaluateDofMan(value, ch, d, tStep) ) {
                answer [ i ] = value;
            }
            break;
        case CK_LoadLevel:
            answer [ i ] = this->emodel->giveLoadLevel();
            break;
        case CK_IP:
            if ( this->evaluateIP(value, ch, d, tStep) ) {
                answer [ i ] = value;
            }
            break;
        case CK_Aggregate:
            if ( this->evaluateAggregate(value, ch, d, tStep) ) {
                answer [ i ] = value;
            }
            break;
        case CK_Reaction:
            hasReactions = true;
            break;
        }
    }

    if ( hasReactions ) {
        this->evaluateReactions(answer, d, tStep);
    }
}


bool
SolutionProgressExportModule :: evaluateDofMan(double &answer, const Channel &ch, Domain *d, TimeStep *tStep)
{
    DofManager *dman = d->giveGlobalDofManager(ch.number);
    if ( !dman || dman->giveParallelMode() == DofManager_remote || dman->giveParallelMode() == DofManager_null ) {
        return false;
    }
    if ( !dman->hasDofID( ( DofIDItem ) ch.dofId ) ) {
        return false;
    }
    answer = dman->giveDofWithID(ch.dofId)->giveUnknown(ch.mode, tStep);
    return true;
}


bool
SolutionProgressExportModule :: evaluateIP(double &answer, const Channel &ch, Domain *d, TimeStep *tStep)
{
    Element *elem = d->giveGlobalElement(ch.number);
    if ( !elem || elem->giveParallelMode() != Element_local ) {
        return false;
    }
    // irule and gp are both 1-based in the input
    if ( ch.irule < 1 || ch.irule > elem->giveNumberOfIntegrationRules() ) {
        return false;
    }
    IntegrationRule *iRule = elem->giveIntegrationRule(ch.irule - 1);
    if ( ch.gp < 1 || ch.gp > iRule->giveNumberOfIntegrationPoints() ) {
        return false;
    }
    FloatArray val;
    if ( !elem->giveIPValue(val, iRule->getIntegrationPoint(ch.gp - 1), ch.ist, tStep) ) {
        return false;
    }
    if ( ch.comp < 1 || ch.comp > val.giveSize() ) {
        return false;
    }
    answer = val.at(ch.comp);
    return true;
}


bool
SolutionProgressExportModule :: evaluateAggregate(double &answer, const Channel &ch, Domain *d, TimeStep *tStep)
{
    if ( ch.number < 1 || ch.number > d->giveNumberOfSets() ) {
        return false;
    }
    FloatArray val;
    int count = 0;
    double result = 0., sumWeights = 0., sumValues = 0.;
    for ( int ielem : d->giveSet(ch.number)->giveElementList() ) {
        Element *elem = d->giveElement(ielem);
        if ( elem->giveParallelMode() != Element_local ) {
            continue;
        }
        IntegrationRule *iRule = elem->giveDefaultIntegrationRulePtr();
        if ( !iRule ) {
            continue;
        }
        for ( GaussPoint *gp : *iRule ) {
            if ( !elem->giveIPValue(val, gp, ch.ist, tStep) || ch.comp > val.giveSize() ) {
                continue;
            }
            double v = val.at(ch.comp);
            if ( ch.op == AO_Average ) {
                double w = elem->computeVolumeAround(gp);
                sumWeights += w;
                result += w * v;
                sumValues += v;
            } else if ( count == 0 ) {
                result = v;
            } else {
                result = ch.op == AO_Max ? std::max(result, v) : std::min(result, v);
            }
            count++;
        }
    }
    if ( count == 0 ) {
        return false;
    }
    if ( ch.op == AO_Average ) {
        // fall back to arithmetic mean if elements do not provide their volume
        result = sumWeights > 0. ? result / sumWeights : sumValues / count;
    }
    answer = result;
    return true;
}


void
SolutionProgressExportModule :: evaluateReactions(std::vector< double > &answer, Domain *d, TimeStep *tStep)
{
#ifdef __SM_MODULE
    StructuralEngngModel *seModel = dynamic_cast< StructuralEngngModel * >( this->emodel );
    if ( !seModel ) {
        return; // values stay NaN (null)
    }

    // reactions of all supported dofs; the table maps reaction index to (local dofman, dofid, prescribed eq.)
    FloatArray reactions;
    IntArray dofManMap, dofIdMap, eqnMap;
    seModel->buildReactionTable(dofManMap, dofIdMap, eqnMap, tStep, d->giveNumber());
    seModel->computeReaction(reactions, tStep, d->giveNumber());

    for ( size_t i = 0; i < this->channels.size(); i++ ) {
        const Channel &ch = this->channels [ i ];
        if ( ch.kind != CK_Reaction ) {
            continue;
        }
        // nodes of the set (sorted for fast lookup); set 0 means all supported nodes
        std::vector< int > nodes;
        if ( ch.number > 0 ) {
            if ( ch.number > d->giveNumberOfSets() ) {
                continue;
            }
            const IntArray &list = d->giveSet(ch.number)->giveNodeList();
            nodes.assign( list.begin(), list.end() );
            std::sort( nodes.begin(), nodes.end() );
        }
        double sum = 0.;
        for ( int j = 1; j <= dofManMap.giveSize(); j++ ) {
            if ( dofIdMap.at(j) != ch.dofId ) {
                continue;
            }
            if ( ch.number > 0 && !std::binary_search( nodes.begin(), nodes.end(), dofManMap.at(j) ) ) {
                continue;
            }
            ///@todo parallel runs: sums are local to the partition (only rank 0 values are written)
            sum += reactions.at( eqnMap.at(j) );
        }
        answer [ i ] = sum;
    }
#endif
}
} // end namespace oofem
