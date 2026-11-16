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

#ifndef outputselection_h_
#define outputselection_h_

#include "oofemenv.h"
#include "intarray.h"

namespace oofem {
class OutputWriter;
class DofManager;
class Element;
class TimeStep;

/**
 * Selection of the components reported by EngngModel::reportOutputAt (structured output).
 * The filter decides which domains, dof managers and elements are reported and which
 * sections (dofmans, elements, problem specific sections such as reactions) are written.
 */
class OOFEM_EXPORT OutputFilter
{
public:
    /// Set of nodes to report for each domain (0 or missing value = all).
    IntArray nodeSets;
    /// Set of elements to report for each domain (0 or missing value = all).
    IntArray elementSets;
    /// Numbers of reported domains (empty = all).
    IntArray domains;
    /// Report the "dofmans" section.
    bool dofManagers = true;
    /// Report the "elements" section.
    bool elements = true;
    /// Report the reaction forces (section "reactions" of problems supporting it).
    bool reactions = true;

    /// Returns the node set for given domain (0 = all nodes).
    int giveNodeSet(int domain) const { return nodeSets.giveSize() < domain ? 0 : nodeSets.at(domain); }
    /// Returns the element set for given domain (0 = all elements).
    int giveElementSet(int domain) const { return elementSets.giveSize() < domain ? 0 : elementSets.at(domain); }
    /// Returns true if given domain is reported.
    bool testDomain(int domain) const { return domains.isEmpty() || domains.contains(domain); }
};


/**
 * Content of the records reported by EngngModel::reportOutputAt (structured output).
 * EngngModel::reportOutputAt traverses the model (domains, dof managers, elements), opens
 * a record for every selected component and lets the content report its fields.
 * The default content is the full state of the component (DofManager::reportOutput,
 * Element::reportOutput), as reported by the output modules; export modules derive
 * their own content reporting only the quantities selected by the user.
 */
class OOFEM_EXPORT OutputContent
{
public:
    virtual ~OutputContent() { }
    /// Reports the fields of given dof manager (the record is already open).
    virtual void reportDofManager(OutputWriter &writer, DofManager *dman, TimeStep *tStep);
    /// Reports the fields of given element (the record is already open).
    virtual void reportElement(OutputWriter &writer, Element *elem, TimeStep *tStep);
};
} // end namespace oofem
#endif // outputselection_h_
