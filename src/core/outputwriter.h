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

#ifndef outputwriter_h_
#define outputwriter_h_

#include "oofemenv.h"

#include <cstdio>
#include <functional>
#include <string>
#include <vector>

namespace oofem {
class FloatArray;
class FloatMatrix;
class IntArray;
class TimeStep;
class EngngModel;

/**
 * Abstract output writer (output driver).
 *
 * Components report their output (see e.g. FEMComponent::reportOutput) by calling
 * the services of the writer, which translates the reported data into a particular
 * format. The interface is a streaming one: the data are passed to the writer as they are
 * reported, so no intermediate containers need to be allocated by the components.
 *
 * The output is organized hierarchically:
 * - the output of a solution step is enclosed by beginStep / endStep,
 * - within a step, the output of each domain is enclosed by beginDomain / endDomain,
 * - within a domain, the components are reported in sections (beginSection / endSection),
 *   e.g. "dofmans", "elements", "reactions",
 * - within a section, the output of a single component (dof manager, element, ...)
 *   is a record, enclosed by beginRecord / endRecord,
 * - within a record, data are reported as named fields; data can be structured
 *   using lists (beginList / endList) of groups (beginGroup / endGroup), for example
 *   a list of integration points of an element, each represented by a group of fields.
 *
 * Components that do not implement structured reporting yet report the text produced
 * by their printOutputAt service using legacyText.
 */
class OOFEM_EXPORT OutputWriter
{
public:
    OutputWriter() { }
    virtual ~OutputWriter() { }

    /// Called at the beginning of the output (before any step output).
    virtual void beginOutput(EngngModel *emodel) { }
    /// Called at the end of the output.
    virtual void endOutput(EngngModel *emodel) { }

    /**
     * Begins the output of the solution step.
     * @param tStep Solution step.
     * @param time Time reported for the step (scaled).
     */
    virtual void beginStep(TimeStep *tStep, double time) = 0;
    /// Ends the output of the solution step.
    virtual void endStep() = 0;
    /// Begins the output of the domain (within a step).
    virtual void beginDomain(int number) = 0;
    /// Ends the output of the domain.
    virtual void endDomain() = 0;
    /**
     * Begins a named section of records within the domain
     * (e.g. "dofmans", "elements", "reactions").
     */
    virtual void beginSection(const char *name) = 0;
    /// Ends the section.
    virtual void endSection() = 0;

    /**
     * Begins the record (the output of a single component) within the current section.
     * @param number Number of the component (0 if not applicable).
     * @param label Label of the component (0 if not applicable).
     * @param className Class name of the component (may be nullptr).
     */
    virtual void beginRecord(int number, int label, const char *className = nullptr) = 0;
    /// Ends the record.
    virtual void endRecord() = 0;

    /// Begins a named list (of groups or values).
    virtual void beginList(const char *name) = 0;
    /// Ends the list.
    virtual void endList() = 0;
    /**
     * Begins a group of fields.
     * @param name Name of the group; nullptr for a group being an item of a list.
     */
    virtual void beginGroup(const char *name = nullptr) = 0;
    /// Ends the group.
    virtual void endGroup() = 0;

    ///@name Fields. The name is ignored for items of lists.
    //@{
    virtual void field(const char *name, bool value) = 0;
    virtual void field(const char *name, int value) = 0;
    virtual void field(const char *name, double value) = 0;
    virtual void field(const char *name, const char *value) = 0;
    virtual void field(const char *name, const std :: string &value) { this->field( name, value.c_str() ); }
    virtual void field(const char *name, const FloatArray &value) = 0;
    virtual void field(const char *name, const IntArray &value) = 0;
    virtual void field(const char *name, const FloatMatrix &value) = 0;
    //@}

    /**
     * Reports the text produced by the (legacy) printOutputAt service of a component,
     * which does not support structured reporting yet.
     */
    virtual void legacyText(const std :: string &text) { this->field("text", text); }
};


/**
 * Writer producing JSON Lines (https://jsonlines.org).
 *
 * The output of every solution step is written as a single JSON object on a separate line,
 * nested according to the structure of the output:
 * @code
 * {"kind":"step","step":1,"time":1.0,"metastep":1,"solution_time":1.0,
 *  "domains":[{"number":1,
 *              "dofmans":[{"number":1,"label":1,"class":"Node",...}, ...],
 *              "elements":[{"number":1,"label":1,"class":"Truss1d",...}, ...]}]}
 * @endcode
 * Sections become arrays of the domain object, records become objects (items of the section array).
 * Besides the step lines, the writer produces the "header" line (at the beginning)
 * and the "footer" line (at the end). Every line is a complete JSON object, so the file can be
 * processed step by step (e.g. using jq), and all finished steps remain readable
 * if the analysis is interrupted.
 *
 * The data are written directly to the stream as they are reported (nothing is buffered
 * by the writer), the stream is flushed by the owner (see JsonFile) at the end of each step.
 */
class OOFEM_EXPORT JsonLinesWriter : public OutputWriter
{
protected:
    FILE *stream;
    /// Stack of nesting levels of the current step line; true if the level is a list, false for an object.
    std :: vector< bool >isList;
    /// Stack of flags indicating that no member has been written at the given level yet.
    std :: vector< bool >isFirst;
    /// True if the "domains" list of the current step has been opened.
    bool domainsOpen;

public:
    JsonLinesWriter(FILE *stream);
    virtual ~JsonLinesWriter() { }

    void beginOutput(EngngModel *emodel) override;
    void endOutput(EngngModel *emodel) override;
    void beginStep(TimeStep *tStep, double time) override;
    void endStep() override;
    void beginDomain(int number) override;
    void endDomain() override;
    void beginSection(const char *name) override { this->beginList(name); }
    void endSection() override { this->endList(); }
    void beginRecord(int number, int label, const char *className = nullptr) override;
    void endRecord() override { this->endGroup(); }
    void beginList(const char *name) override;
    void endList() override;
    void beginGroup(const char *name = nullptr) override;
    void endGroup() override;

    using OutputWriter :: field;
    void field(const char *name, bool value) override;
    void field(const char *name, int value) override;
    void field(const char *name, double value) override;
    void field(const char *name, const char *value) override;
    void field(const char *name, const FloatArray &value) override;
    void field(const char *name, const IntArray &value) override;
    void field(const char *name, const FloatMatrix &value) override;

    /// Escapes the string to be used in JSON.
    static std :: string escape(const char *str);
    /// Formats the number for JSON (non-finite numbers are written as null).
    static std :: string number(double value);

protected:
    /// Writes given text to the stream.
    void put(const std :: string &text) { fputs(text.c_str(), stream); }
    /// Writes the separator (if needed) and the name of the member (if in object).
    void key(const char *name);
    /// Writes the separator and the opening bracket of a new item of the current list, and pushes the level.
    void open(const char *name, bool list);
    /// Writes the closing bracket of the current level and pops it.
    void close(bool list);
    /// Returns true if the current level is managed by the step/domain services (can not be closed by endList/endGroup).
    bool isReservedLevel() const;
    /// Writes a complete stand-alone line with given kind and members (header, footer).
    void writeLine(const char *kind, const std :: string &members);
};


/**
 * Captures the text written to a stream by the given function.
 * Used to report the output of components which support only the (legacy)
 * printOutputAt(FILE *, ...) service.
 */
OOFEM_EXPORT std :: string captureOutput(const std :: function< void(FILE *) > &printer);
} // end namespace oofem
#endif // outputwriter_h_
