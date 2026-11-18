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

#include "exportmodulemanager.h"
#include "modulemanager.h"
#include "exportmodule.h"
#include "classfactory.h"
#include "error.h"
#include "textoutputmodule.h"

namespace oofem {
ExportModuleManager :: ExportModuleManager(EngngModel *emodel) : ModuleManager< ExportModule >(emodel)
{ }

ExportModuleManager :: ~ExportModuleManager()
{ }

std::unique_ptr<ExportModule> ExportModuleManager :: CreateModule(const char *name, int n, EngngModel *emodel)
{
    return classFactory.createExportModule(name, n, emodel);
}

void
ExportModuleManager :: registerModule(std :: unique_ptr< ExportModule > &module)
{
    if ( dynamic_cast< OutputModule * >( module.get() ) ) {
        if ( dynamic_cast< TextOutputModule * >( module.get() ) ) {
            auto *existing = this->giveTextOutputModule();
            if ( existing ) {
                if ( !existing->isImplicit() ) {
                    OOFEM_ERROR("Only one text output module can be defined");
                }
                // explicit module replaces the implicit (default) one
                for ( auto it = outputModuleList.begin(); it != outputModuleList.end(); ++it ) {
                    if ( it->get() == existing ) {
                        outputModuleList.erase(it);
                        break;
                    }
                }
            }
        }
        outputModuleList.emplace_back( static_cast< OutputModule * >( module.release() ) );
    } else {
        ModuleManager< ExportModule > :: registerModule(module);
    }
}


OutputModule *
ExportModuleManager :: giveOutputModule(int i)
{
    if ( i < 1 || i > (int)outputModuleList.size() ) {
        OOFEM_ERROR("No output module no. %d defined", i);
    }
    return outputModuleList [ i - 1 ].get();
}


TextOutputModule *
ExportModuleManager :: giveTextOutputModule()
{
    for ( auto &module: outputModuleList ) {
        if ( auto *tom = dynamic_cast< TextOutputModule * >( module.get() ) ) {
            return tom;
        }
    }
    return nullptr;
}


void
ExportModuleManager :: doOutput(TimeStep *tStep, bool substepFlag)
{
    this->doOutputModulesOutput(tStep, substepFlag);
    this->doExportModulesOutput(tStep, substepFlag);
}


void
ExportModuleManager :: doOutputModulesOutput(TimeStep *tStep, bool substepFlag)
{
    for ( auto &module: outputModuleList ) {
        if ( !substepFlag || module->testSubStepOutput() ) {
            module->doOutput(tStep);
        }
    }
}


void
ExportModuleManager :: doExportModulesOutput(TimeStep *tStep, bool substepFlag)
{
    for ( auto &module: moduleList ) {
        if ( !substepFlag || module->testSubStepOutput() ) {
            module->doOutput(tStep);
        }
    }
}


void
ExportModuleManager :: doIterationOutput(TimeStep *tStep, const SolverIterationInfo &info)
{
    for ( auto &module: moduleList ) {
        module->doIterationOutput(tStep, info);
    }
}

void
ExportModuleManager :: initialize()
{
    for ( auto &module: outputModuleList ) {
        module->initialize();
    }
    for ( auto &module: moduleList ) {
        module->initialize();
    }
}


void
ExportModuleManager :: terminate()
{
    for ( auto &module: outputModuleList ) {
        module->terminate();
    }
    for ( auto &module: moduleList ) {
        module->terminate();
    }
}
} // end namespace oofem
