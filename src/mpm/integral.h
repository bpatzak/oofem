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
#ifndef integral_h
#define integral_h

#include "mpm.h"
#include "set.h"
#include "intarray.h"
#include "domain.h"
#include "sparsemtrx.h"

#include <vector>
#include <unordered_map>

namespace oofem {

    /**
    * @brief Class representing weak form integral
    * 
    */
   class Integral {
    public:

        Set* set;
        int setIndex=0;
        const Term *term;
        Domain *domain;
        double factor;
        /// @brief Constructor, creates an integral of given term over entities in given set 
        /// @param d
        /// @param s 
        /// @param t 
        Integral (Domain* d, Set* set, const Term* t ) : set(set), term(t), factor(1.0) {
            this->domain = d;
        }   
        void initializeFrom (const std::shared_ptr<InputRecord> &ir, EngngModel *emodel);
        /// @brief Initialize the integral domain 
        void initialize() {
            if (this->set == nullptr) {
                 this->set = this->domain->giveSet(this->setIndex);
            }
            for (auto i: this->set->giveElementList()) { // loop over elements
                Element *e = domain->giveElement(i);
                // introduce necessary dofs and set-up integration rules
                this->term->initializeCell(*e);
                // Record this term's unknown field as a source of state quantities on the cell, so
                // that the cell can later assemble the generalized state vector its material asks
                // for. Done per cell, so cells carrying different materials (and therefore
                // different state layouts, fed by different fields) each get their own resolution.
                //
                // Only genuine unknowns qualify. A deck may name a test function as a term's
                // variable -- pure source terms, with a zero lexpression, have no unknown to
                // depend on and something has to go there -- and reading nodal unknowns through a
                // weighting function's interpolation would be wrong wherever the two differ.
                if (this->term->field && !this->term->field->isTestField()) {
                    if (MPElement *cell = dynamic_cast<MPElement*>(e)) {
                        cell->registerStateVariable(this->term->field);
                    }
                }
            }
        }
        // evaluate term contribution to weak form on given cell at given point 
        void assemble_lhs (SparseMtrx& dest, const UnknownNumberingScheme &s, TimeStep* tStep, double factor2 = 1.0) const {
            for (auto i: this->set->giveElementList()) { // loop over elements
                MPElement *e = dynamic_cast<MPElement*>(this->domain->giveElement(i));
                if (e) {
                    this->assembleElement_lhs(dest, e, s, tStep, factor2);
                }
            }
        }
        // evaluate contribution (all vars known) on given cell
        void assemble_rhs (FloatArray& dest, const UnknownNumberingScheme &s, TimeStep* tstep, FloatArray* eNorms=NULL, double factor2 = 1.0) const {
            for (auto i: this->set->giveElementList()) { // loop over elements
                MPElement *e = dynamic_cast<MPElement*>(this->domain->giveElement(i));
                if (e) {
                    this->assembleElement_rhs(dest, e, s, tstep, eNorms, factor2);
                }
            }
        }
        /// Assembles the contribution of the term on one element of the integration domain to dest (see assemble_lhs).
        void assembleElement_lhs (SparseMtrx& dest, MPElement *e, const UnknownNumberingScheme &s, TimeStep* tStep, double factor2 = 1.0) const {
            IntArray locr, locc;
            FloatMatrix contrib;
            this->getElementTermCodeNumbers(locr, locc, e, *this->term, s);
            // determine integration rule (this has to be set up on element, as we need to track history there)
            // the IR is created by term.initialize, we just need mechanism to get this IR
            // specific terms can have specific integration requirements (reduced integration, etc)
            // at the same time same rules should be shared between terms->
            // ->need to querry/store/identify element IR based on NIP.
            IntegrationRule* ir =  this->term->giveElementIntegrationRule(e);
            e->integrateTerm_dw(contrib, *this->term, ir, tStep); // @todo IR 
            if (contrib.isNotEmpty()) {
                contrib.times(this->factor * factor2);
                // assemble
                dest.assemble (locr, locc, contrib);
            }
        }
        /// Assembles the contribution of the term on one element of the integration domain to dest (see assemble_rhs).
        void assembleElement_rhs (FloatArray& dest, MPElement *e, const UnknownNumberingScheme &s, TimeStep* tstep, FloatArray* eNorms=NULL, double factor2 = 1.0) const {
            IntArray locr, locc, dofIDs;
            FloatArray contrib;
            this->getElementTermCodeNumbers(locr, locc, e, *this->term, s, &dofIDs);
            // determine integration rule (this has to be set up on element, as we need to track history there)
            // the IR is created by term.initialize, we just need mechanism to get this IR
            // specific terms can have specific integration requirements (reduced integration, etc)
            // at the same time same rules should be shared between terms->
            // ->need to querry/store/identify element IR based on NIP.
            IntegrationRule* ir =  this->term->giveElementIntegrationRule(e);
            e->integrateTerm_c(contrib, *this->term, ir, tstep); // @todo IR
            contrib.times(this->factor * factor2);
            // assemble
            if (contrib.isNotEmpty()) {
                dest.assemble (contrib, locr);
            }
            if ( eNorms ) {
                eNorms->assembleSquared(contrib, dofIDs);
            }
        }
        /// A member of a group of integrals assembled element by element (see assembleGroup_lhs/rhs): the integral,
        /// its additional factor and, for the rhs, the vector of element contribution norms (or nullptr).
        struct GroupMember {
            const Integral *integral;
            double factor2 = 1.0;
            FloatArray *eNorms = nullptr;
        };
        /**
         * Assembles several integrals element by element. Each element of their integration domains evaluates all
         * integrals acting on it within one MPElement::EvaluationScope, so that their terms share the gathered
         * unknowns, dN/dx and the volume measure, instead of every integral sweeping over all elements. The same as
         * assemble_lhs of each member (with its factor2), up to the order in which contributions are summed.
         */
        static void assembleGroup_lhs (SparseMtrx& dest, const std::vector<GroupMember> &group, const UnknownNumberingScheme &s, TimeStep* tStep) {
            for ( const auto &[elem, members] : groupByElement(group) ) {
                if ( MPElement *e = dynamic_cast<MPElement*>(elem) ) {
                    MPElement::EvaluationScope evalScope(*e, true);
                    for ( int m : members ) {
                        group[m].integral->assembleElement_lhs(dest, e, s, tStep, group[m].factor2);
                    }
                }
            }
        }
        /// The rhs counterpart of assembleGroup_lhs (see assemble_rhs).
        static void assembleGroup_rhs (FloatArray& dest, const std::vector<GroupMember> &group, const UnknownNumberingScheme &s, TimeStep* tstep) {
            for ( const auto &[elem, members] : groupByElement(group) ) {
                if ( MPElement *e = dynamic_cast<MPElement*>(elem) ) {
                    MPElement::EvaluationScope evalScope(*e, true);
                    for ( int m : members ) {
                        group[m].integral->assembleElement_rhs(dest, e, s, tstep, group[m].eNorms, group[m].factor2);
                    }
                }
            }
        }
        /**
         * Elements of the integration domains of group, in the order of their first appearance, each with the indices
         * of the members acting on it in the order of group (an element listed repeatedly in a set keeps its repeats).
         */
        static std::vector< std::pair< Element*, std::vector<int> > > groupByElement (const std::vector<GroupMember> &group) {
            std::vector< std::pair< Element*, std::vector<int> > > answer;
            std::unordered_map< Element*, std::size_t > position;
            for ( std::size_t m = 0; m < group.size(); m++ ) {
                const Integral *integral = group[m].integral;
                for ( int i : integral->set->giveElementList() ) {
                    Element *elem = integral->domain->giveElement(i);
                    auto it = position.find(elem);
                    if ( it == position.end() ) {
                        it = position.emplace(elem, answer.size()).first;
                        answer.push_back({ elem, {} });
                    }
                    answer[ it->second ].second.push_back( (int) m );
                }
            }
            return answer;
        }
        void getElementTermCodeNumbers (IntArray &locr, IntArray &locc, Element* e, const Term& t, const UnknownNumberingScheme &s, IntArray* rowDofIDs=NULL) const {
            IntArray nodes, internalDofMans, loc, masterDofIDs;
            locr.resize(0);
            locc.resize(0);
            // term.field and its interpolation determines column code numbers
            // from interpolation get node and internalDofMan lists
            t.field->interpolation->giveCellDofMans(nodes, internalDofMans, e);
            // loop over dof managers to get code numbers
            for (int i: nodes) {
                // get mode dofID mask
                e->giveDofManager(i)->giveLocationArray(t.field->getDofManDofIDs(), loc, s);
                locc.followedBy(loc);
            }
            for (int i: internalDofMans) {
                e->giveInternalDofManager(i)->giveLocationArray(t.field->getDofManDofIDs(), loc, s);
                locc.followedBy(loc);
            }

            if ( rowDofIDs ) {
                rowDofIDs->clear();
            }
            // term.testField and its interpolation determines row code numbers
            // from interpolation get node and internalDofMan lists
            t.testField->interpolation->giveCellDofMans(nodes, internalDofMans, e);
            // loop over dof managers to get code numbers
            for (int i: nodes) {
                // get mode dofID mask
                e->giveDofManager(i)->giveLocationArray(t.testField->getDofManDofIDs(), loc, s);
                locr.followedBy(loc);
                if ( rowDofIDs ) {
                    e->giveDofManager(i)->giveMasterDofIDArray(t.testField->getDofManDofIDs(), masterDofIDs);
                    rowDofIDs->followedBy(masterDofIDs);
                }
            }
            for (int i: internalDofMans) {
                e->giveInternalDofManager(i)->giveLocationArray(t.testField->getDofManDofIDs(), loc, s);
                locr.followedBy(loc);
                if ( rowDofIDs ) {
                    e->giveInternalDofManager(i)->giveMasterDofIDArray(t.testField->getDofManDofIDs(), masterDofIDs);
                    rowDofIDs->followedBy(masterDofIDs);
                }
            }
        }
   };

} // namespace oofem

#endif // integral_h
