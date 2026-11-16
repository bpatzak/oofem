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

#ifndef solutionprogressexportmodule_h
#define solutionprogressexportmodule_h

#include "exportmodule.h"
#include "valuemodetype.h"
#include "internalstatetype.h"

#include <chrono>
#include <cstdio>
#include <string>
#include <vector>

///@name Input fields for SolutionProgressExportModule
//@{
#define _IFT_SolutionProgressExportModule_Name "progress"
#define _IFT_SolutionProgressExportModule_filename "filename" ///< Output file name (default: <output base>.progress.jsonl)
#define _IFT_SolutionProgressExportModule_iterations "iterations" ///< Flag: write nonlinear solver iteration records
#define _IFT_SolutionProgressExportModule_dofmanIds "dofman_ids" ///< DOF manager numbers of dofman channels
#define _IFT_SolutionProgressExportModule_dofmanDofs "dofman_dofs" ///< DofIDs of dofman channels
#define _IFT_SolutionProgressExportModule_dofmanModes "dofman_modes" ///< Optional value modes (0=total, 1=velocity, 2=acceleration)
#define _IFT_SolutionProgressExportModule_reactionDofs "reaction_dofs" ///< DofIDs of reaction-sum channels
#define _IFT_SolutionProgressExportModule_reactionSets "reaction_sets" ///< Node sets of reaction-sum channels (0 = all supported nodes)
#define _IFT_SolutionProgressExportModule_loadLevel "loadlevel" ///< Flag: add load level channel
#define _IFT_SolutionProgressExportModule_ipElems "ip_elems" ///< Element numbers of integration-point channels
#define _IFT_SolutionProgressExportModule_ipRules "ip_rules" ///< Optional integration rule numbers (default 1)
#define _IFT_SolutionProgressExportModule_ipGps "ip_gps" ///< Integration point numbers (1-based)
#define _IFT_SolutionProgressExportModule_ipIsts "ip_ists" ///< Internal state types (InternalStateType values)
#define _IFT_SolutionProgressExportModule_ipComps "ip_comps" ///< Optional components (1-based, default 1)
#define _IFT_SolutionProgressExportModule_aggSets "agg_sets" ///< Element sets of aggregate channels
#define _IFT_SolutionProgressExportModule_aggIsts "agg_ists" ///< Internal state types of aggregate channels
#define _IFT_SolutionProgressExportModule_aggComps "agg_comps" ///< Optional components (1-based, default 1)
#define _IFT_SolutionProgressExportModule_aggOps "agg_ops" ///< Optional operations (0=max, 1=min, 2=volume average; default 0)
#define _IFT_SolutionProgressExportModule_labels "labels" ///< Optional channel labels overriding the generated ones
#define _IFT_SolutionProgressExportModule_plotX "plot_x" ///< Optional default charts: x channel indices (0 = time)
#define _IFT_SolutionProgressExportModule_plotY "plot_y" ///< Optional default charts: y channel indices (0 = time)
//@}

namespace oofem {
class Domain;

/**
 * Writes the progress of the solution into a file that can be followed by an external viewer
 * (tools/oofemprogress), e.g. to show an evolving load-displacement diagram while the job runs.
 *
 * The file uses the "oofem-progress" protocol: JSON Lines, one self-contained record per line,
 * flushed after every record, so that a reader can tail the file while it is being written.
 * Record types:
 *  - header: written by initialize(); identifies the job (OOFEM version, input, host, pid, start time)
 *    and describes the monitored channels and the suggested default charts,
 *  - iter: one per nonlinear solver iteration (only with the "iterations" flag), with error norms,
 *  - step: one per (selected) solution step, with time, solver status and the channel values,
 *  - end: written by terminate(), when the analysis finishes normally.
 * The viewer detects an abnormal termination by a missing end record and a dead process.
 *
 * The monitored quantities (channels) are scalars, specified in the module record:
 *  - DOF manager unknowns (displacement, velocity, acceleration of a given DOF),
 *  - sum of reactions of given DOF over a node set (structural problems only),
 *  - load level,
 *  - value of internal state variable component at a single integration point,
 *  - aggregate (max, min, volume average) of internal state variable component over an element set.
 */
class OOFEM_EXPORT SolutionProgressExportModule : public ExportModule
{
public:
    /// Kind of monitored quantity.
    enum ChannelKind { CK_DofMan, CK_Reaction, CK_LoadLevel, CK_IP, CK_Aggregate };
    /// Aggregation operation over element set.
    enum AggregateOp { AO_Max = 0, AO_Min = 1, AO_Average = 2 };

    /// Description of a single scalar monitored quantity.
    struct Channel {
        ChannelKind kind = CK_LoadLevel;
        std::string id;      ///< Unique label used to reference the channel in the viewer.
        int number = 0;      ///< DOF manager or element number, set number for reaction and aggregate channels.
        int dofId = 0;       ///< DofID (dofman and reaction channels).
        ValueModeType mode = VM_Total; ///< Value mode (dofman channels).
        int irule = 1;       ///< Integration rule number (IP channels).
        int gp = 1;          ///< Integration point number, 1-based (IP channels).
        InternalStateType ist = IST_Undefined; ///< Internal state type (IP and aggregate channels).
        int comp = 1;        ///< Component of internal state, 1-based (IP and aggregate channels).
        AggregateOp op = AO_Max; ///< Operation (aggregate channels).
    };

protected:
    /// Name of the output file.
    std::string filename;
    /// Output stream, open between initialize() and terminate().
    FILE *stream = nullptr;
    /// When true, iteration records are written.
    bool iterationsFlag = false;
    /// Monitored channels, in order of the values in step records.
    std::vector< Channel > channels;
    /// Suggested default charts (pairs of channel indices, 0 = time).
    IntArray plotX, plotY;
    /// Wall clock time of initialization, used to report elapsed time.
    std::chrono::steady_clock::time_point startTime;

public:
    SolutionProgressExportModule(int n, EngngModel *e);
    SolutionProgressExportModule(const SolutionProgressExportModule &) = delete;
    SolutionProgressExportModule &operator=(const SolutionProgressExportModule &) = delete;
    virtual ~SolutionProgressExportModule();

    void initializeFrom(const std::shared_ptr<InputRecord> &ir) override;
    void initialize() override;
    void doOutput(TimeStep *tStep, bool forcedOutput = false) override;
    void doIterationOutput(TimeStep *tStep, const SolverIterationInfo &info) override;
    void terminate() override;

    const char *giveClassName() const override { return "SolutionProgressExportModule"; }
    const char *giveInputRecordName() const { return _IFT_SolutionProgressExportModule_Name; }

    /// Returns the monitored channels (useful for testing).
    const std::vector< Channel > &giveChannels() const { return channels; }

protected:
    /// Returns true if this process writes the file (serial run or rank 0).
    bool isWriter() const;
    /// Seconds elapsed since initialize().
    double giveElapsedTime() const;
    /// Writes a complete record (one line) and flushes the stream.
    void writeRecord(const std::string &record);
    /// Builds the header record.
    std::string giveHeaderRecord();
    /**
     * Evaluates all channels for given step.
     * @param answer Values in order of channels; NaN for values that could not be evaluated.
     */
    void evaluateChannels(std::vector< double > &answer, TimeStep *tStep);
    /// Evaluates a dofman channel; returns false if not available.
    bool evaluateDofMan(double &answer, const Channel &ch, Domain *d, TimeStep *tStep);
    /// Evaluates an integration point channel; returns false if not available.
    bool evaluateIP(double &answer, const Channel &ch, Domain *d, TimeStep *tStep);
    /// Evaluates an aggregate channel; returns false if no integration point contributed.
    bool evaluateAggregate(double &answer, const Channel &ch, Domain *d, TimeStep *tStep);
    /// Evaluates all reaction channels at once (they share the expensive reaction computation).
    void evaluateReactions(std::vector< double > &answer, Domain *d, TimeStep *tStep);
};
} // end namespace oofem
#endif // solutionprogressexportmodule_h
