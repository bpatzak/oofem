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

#ifndef outputmodule_h_
#define outputmodule_h_

#include "exportmodule.h"

namespace oofem {
/**
 * Base class for output modules.
 *
 * Output modules are export modules that report the full state of the model:
 * every component (dof manager, element, integration point, material status, ...)
 * reports everything it knows, and the module decides only which time steps,
 * nodes and elements are included and in which format the data are written.
 * Typical examples are the standard text output (the .out file) and the
 * structured (JSON) output.
 *
 * This is in contrast to plain export modules (VTK, Matlab, ...), which export
 * a selection of quantities chosen by the user and query them from the model.
 *
 * Output modules are managed together with export modules by the
 * ExportModuleManager, they are declared in the same input section, and they are
 * always processed before the plain export modules.
 */
class OOFEM_EXPORT OutputModule : public ExportModule
{
public:
    OutputModule(int n, EngngModel *e) : ExportModule(n, e) { }
    virtual ~OutputModule() { }
};
} // end namespace oofem
#endif // outputmodule_h_
