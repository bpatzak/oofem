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

#ifndef jsonfile_h_
#define jsonfile_h_

#include "oofemenv.h"

#include <cstdio>
#include <memory>
#include <string>

namespace oofem {
class EngngModel;
class TimeStep;
class JsonLinesWriter;

/**
 * JSON Lines output file shared by the JSON output modules (JsonOutputModule, JsonExportModule).
 *
 * The class owns the output stream and the writer, so that the modules only decide
 * what is reported. The file is opened on first request (the "header" line is written),
 * each output step is enclosed by beginStep / endStep, and close writes the "footer" line.
 * The default file name is <output file>.m<module number>.jsonl, with the rank appended
 * in parallel runs (<output file>.m<module number>.<rank>.jsonl); it can be overridden
 * by setFileName (the filename keyword of the modules).
 */
class OOFEM_EXPORT JsonFile
{
protected:
    /// Engineering model the output belongs to.
    EngngModel *emodel;
    /// Number of the owning module (used in the default file name).
    int moduleNumber;
    /// User defined file name (empty = default).
    std :: string fileName;
    /// Output stream (nullptr until opened).
    FILE *stream;
    /// Writer (nullptr until the stream is opened).
    std :: unique_ptr< JsonLinesWriter >writer;

public:
    JsonFile(EngngModel *emodel, int moduleNumber);
    ~JsonFile();

    /// Sets the user defined file name (empty string restores the default name).
    void setFileName(const std :: string &name) { fileName = name; }
    /// Returns the name of the output file.
    std :: string giveFileName() const;
    /// Returns the writer, opening the file (and writing the header) on first request.
    JsonLinesWriter &giveWriter();
    /**
     * Begins the output of given solution step and returns the writer.
     * The reported time is the target time of the step scaled by the time scale
     * of the model (VST_Time) and by given time scale of the module.
     */
    JsonLinesWriter &beginStep(TimeStep *tStep, double timeScale);
    /// Ends the output of the solution step and flushes the file (every finished step is complete on disk).
    void endStep();
    /// Writes the footer and closes the file (the file is created even if no step was reported).
    void close();
};
} // end namespace oofem
#endif // jsonfile_h_
