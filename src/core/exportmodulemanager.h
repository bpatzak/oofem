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

#ifndef exportmodulemanager_h
#define exportmodulemanager_h

#include "modulemanager.h"
#include "exportmodule.h"
#include "outputmodule.h"

#include <vector>
#include <memory>

namespace oofem {
class EngngModel;

/**
 * Class representing and implementing ExportModuleManager. It is attribute of EngngModel.
 * It manages the export output modules, which perform module - specific output operations.
 */
class OOFEM_EXPORT ExportModuleManager : public ModuleManager< ExportModule >
{
protected:
    /**
     * List of output modules (modules reporting the full state of the model, see OutputModule).
     * Output modules are kept separately from export modules (stored in moduleList),
     * so that numbering of export modules is not affected by them and they can be processed first.
     */
    std :: vector< std :: unique_ptr< OutputModule > >outputModuleList;

public:
    ExportModuleManager(EngngModel * emodel);
    virtual ~ExportModuleManager();

    std::unique_ptr<ExportModule> CreateModule(const char *name, int num, EngngModel *emodel) override;
    /**
     * Registers the module. Output modules (derived from OutputModule) are stored
     * in the list of output modules, the others in the list of export modules.
     */
    void registerModule(std :: unique_ptr< ExportModule > &module) override;

    /**
     * Writes the output. Output modules are processed first, followed by export modules.
     * @param tStep Time step.
     * @param substepFlag If true, only modules requesting substep output are processed.
     */
    void doOutput(TimeStep *tStep, bool substepFlag = false);
    /// Writes the output of output modules only.
    void doOutputModulesOutput(TimeStep *tStep, bool substepFlag = false);
    /// Writes the output of export modules only.
    void doExportModulesOutput(TimeStep *tStep, bool substepFlag = false);
    /**
     * Initializes output modules.
     */
    void initialize();
    /**
     * Terminates the receiver, the corresponding terminate module services are called.
     */
    void terminate();

    /// Returns number of output modules.
    int giveNumberOfOutputModules() const { return (int)outputModuleList.size(); }
    /// Returns the i-th output module (1-based).
    OutputModule *giveOutputModule(int i);

    const char *giveClassName() const override { return "ExportModuleManager"; }
};
} // end namespace oofem
#endif // exportmodulemanager_h
