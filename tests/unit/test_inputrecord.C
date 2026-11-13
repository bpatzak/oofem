/*
 * Unit tests of array and matrix parsing in the text (OOFEMTXTInputRecord)
 * and XML (XMLInputRecord) input records, in particular the Matlab-style
 * bracketed syntax "[1 2 3]" / "[1 2; 3 4]".
 */
#include "oofemtxtinputrecord.h"
#include "inputrecord.h"
#include "intarray.h"
#include "floatarray.h"
#include "floatmatrix.h"
#include "error.h"
#ifdef _USE_XML
 #include "xmldatareader.h"
#endif

#include <catch2/catch_test_macros.hpp>
#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <vector>

using namespace oofem;

namespace {
std::shared_ptr< OOFEMTXTInputRecord > txt(const std::string &line)
{
    return std::make_shared< OOFEMTXTInputRecord >(1, line);
}

IntArray readIntArray(const std::string &line)
{
    IntArray a;
    txt(line)->giveField(a, "a");
    return a;
}

FloatArray readFloatArray(const std::string &line)
{
    FloatArray a;
    txt(line)->giveField(a, "a");
    return a;
}

FloatMatrix readMatrix(const std::string &line)
{
    FloatMatrix m;
    txt(line)->giveField(m, "m");
    return m;
}

bool equal(const IntArray &a, const std::vector< int > &b)
{
    if ( a.giveSize() != ( int ) b.size() ) {
        return false;
    }
    for ( int i = 0; i < a.giveSize(); i++ ) {
        if ( a [ i ] != b [ i ] ) {
            return false;
        }
    }
    return true;
}

bool equal(const FloatArray &a, const std::vector< double > &b)
{
    if ( a.giveSize() != ( int ) b.size() ) {
        return false;
    }
    for ( int i = 0; i < a.giveSize(); i++ ) {
        if ( a [ i ] != b [ i ] ) {
            return false;
        }
    }
    return true;
}

bool equal(const FloatMatrix &a, const FloatMatrix &b)
{
    if ( a.giveNumberOfRows() != b.giveNumberOfRows() || a.giveNumberOfColumns() != b.giveNumberOfColumns() ) {
        return false;
    }
    for ( int i = 1; i <= a.giveNumberOfRows(); i++ ) {
        for ( int j = 1; j <= a.giveNumberOfColumns(); j++ ) {
            if ( a.at(i, j) != b.at(i, j) ) {
                return false;
            }
        }
    }
    return true;
}
} // end anonymous namespace


TEST_CASE("txt: bracketed int array equals size-prefixed one", "[inputrecord][txt]")
{
    std::vector< int >expected { 1, 4, 23 };
    CHECK(equal(readIntArray("Rec 1 a 3 1 4 23"), expected) );
    CHECK(equal(readIntArray("Rec 1 a [1 4 23]"), expected) );
    CHECK(equal(readIntArray("Rec 1 a [1, 4, 23]"), expected) );
    CHECK(equal(readIntArray("Rec 1 a [ 1,4 ,  23 ]"), expected) );
    CHECK(equal(readIntArray("Rec 1 a [-1 0]"), { -1, 0 }) );
}

TEST_CASE("txt: bracketed real array equals size-prefixed one", "[inputrecord][txt]")
{
    std::vector< double >expected { 1.0, -2.5, 3e-3 };
    CHECK(equal(readFloatArray("Rec 1 a 3 1.0 -2.5 3e-3"), expected) );
    CHECK(equal(readFloatArray("Rec 1 a [1.0 -2.5 3e-3]"), expected) );
    CHECK(equal(readFloatArray("Rec 1 a [1.0,-2.5,3e-3]"), expected) );
}

TEST_CASE("txt: empty bracketed arrays", "[inputrecord][txt]")
{
    CHECK(readIntArray("Rec 1 a []").giveSize() == 0);
    CHECK(readFloatArray("Rec 1 a [ ]").giveSize() == 0);
}

TEST_CASE("txt: coordinates accept bracketed values", "[inputrecord][txt]")
{
    Coordinates c;
    txt("Node 1 coords [1 2 3]")->giveField(c, "coords");
    CHECK(c [ 0 ] == 1.0);
    CHECK(c [ 1 ] == 2.0);
    CHECK(c [ 2 ] == 3.0);
}

TEST_CASE("txt: bracketed matrix equals sized one", "[inputrecord][txt]")
{
    FloatMatrix expected = readMatrix("Rec 1 m 2 3 {1.0 -1.0 0.0; 2.0 2.5 5.0}");
    REQUIRE(expected.giveNumberOfRows() == 2);
    REQUIRE(expected.giveNumberOfColumns() == 3);
    CHECK(expected.at(2, 2) == 2.5);

    CHECK(equal(readMatrix("Rec 1 m [1.0 -1.0 0.0; 2.0 2.5 5.0]"), expected) );
    CHECK(equal(readMatrix("Rec 1 m [1.0, -1.0, 0.0; 2.0, 2.5, 5.0]"), expected) );
    CHECK(equal(readMatrix("Rec 1 m [1.0 -1.0 0.0; 2.0 2.5 5.0;]"), expected) ); // trailing ';'
    CHECK(readMatrix("Rec 1 m []").giveNumberOfRows() == 0);
    // single row / single column
    CHECK(readMatrix("Rec 1 m [1 2 3]").giveNumberOfColumns() == 3);
    CHECK(readMatrix("Rec 1 m [1; 2; 3]").giveNumberOfRows() == 3);
}

TEST_CASE("txt: sized {} matrix accepts commas", "[inputrecord][txt]")
{
    FloatMatrix m = readMatrix("Rec 1 m 2 2 {1.0, 2.0; 3.0, 4.0}");
    CHECK(m.at(1, 2) == 2.0);
    CHECK(m.at(2, 1) == 3.0);
    CHECK(m.at(2, 2) == 4.0);
}

TEST_CASE("txt: malformed values are rejected", "[inputrecord][txt]")
{
    CHECK_THROWS_AS(readIntArray("Rec 1 a [1 2.5]"), BadFormatInputException);  // non-integer entry
    CHECK_THROWS_AS(readIntArray("Rec 1 a [1 2; 3 4]"), BadFormatInputException); // matrix given for array
    CHECK_THROWS_AS(readFloatArray("Rec 1 a [1 x]"), BadFormatInputException);
    CHECK_THROWS_AS(readFloatArray("Rec 1 a [1,,2]"), BadFormatInputException);
    CHECK_THROWS_AS(readFloatArray("Rec 1 a [1 2,]"), BadFormatInputException);
    CHECK_THROWS_AS(readMatrix("Rec 1 m [1 2; 3]"), BadFormatInputException);     // ragged rows
    CHECK_THROWS_AS(readMatrix("Rec 1 m [1 2;; 3 4]"), BadFormatInputException);  // empty row
    CHECK_THROWS_AS(readMatrix("Rec 1 m 2 2 {1 2; 3}"), BadFormatInputException); // short row (used to be padded with 0)
    CHECK_THROWS_AS(readMatrix("Rec 1 m 3 2 {1 2; 3 4}"), BadFormatInputException); // row count mismatch
}

TEST_CASE("txt: fields after a bracketed value are found and all tokens read", "[inputrecord][txt]")
{
    auto ir = txt("Rec 1 a [1 2] m [1 0; 0 1] b 7");
    IntArray a;
    FloatMatrix m;
    int b = 0;
    ir->giveField(a, "a");
    ir->giveField(m, "m");
    ir->giveField(b, "b");
    std::string kwd;
    int num;
    ir->giveRecordKeywordField(kwd, num);
    CHECK(equal(a, { 1, 2 }) );
    CHECK(m.giveNumberOfRows() == 2);
    CHECK(b == 7);
    CHECK(kwd == "Rec");
    CHECK(num == 1);
    // nothing should be reported as unread; finish() only warns, so just make sure it does not throw
    CHECK_NOTHROW(ir->finish(true) );
}

TEST_CASE("txt: brackets inside quoted strings are not array tokens", "[inputrecord][txt]")
{
    auto ir = txt("Rec 1 expr \"f[1]=1.0\" a [3]");
    std::string expr;
    IntArray a;
    ir->giveField(expr, "expr");
    ir->giveField(a, "a");
    CHECK(expr == "f[1]=1.0");
    CHECK(equal(a, { 3 }) );
}


#ifdef _USE_XML
namespace {
/// Writes an XML input whose top <oofem> element carries the given attributes and returns its top record.
std::shared_ptr< InputRecord > xmlTop(std::shared_ptr< XMLDataReader > &reader, const std::string &attributes)
{
    static int counter = 0;
    auto path = std::filesystem::temp_directory_path() / ( "oofem_test_inputrecord_" + std::to_string(counter++) + ".xml" );
    {
        std::ofstream f(path);
        f << "<oofem version=\"2\" " << attributes << ">\n"
          << "  <Output>test.out</Output>\n  <Description>input record test</Description>\n"
          << "</oofem>\n";
    }
    reader = std::make_shared< XMLDataReader >(path.string() );
    std::filesystem::remove(path);
    return reader->giveTopInputRecord();
}
} // end anonymous namespace

TEST_CASE("xml: bracketed arrays and matrices equal plain ones", "[inputrecord][xml]")
{
    std::shared_ptr< XMLDataReader >reader;
    auto ir = xmlTop(reader,
                     "a1=\"1 4 23\" a2=\"[1 4 23]\" a3=\"[ 1, 4,23 ]\" a4=\"1,4,23\" "
                     "f1=\"1.0 -2.5\" f2=\"[1.0, -2.5]\" "
                     "m1=\"1 2 3; 4 5 6\" m2=\"[1 2 3; 4 5 6]\" m3=\"[1, 2, 3; 4, 5, 6]\" "
                     "c=\"[1 2 3]\" e=\"[]\" em=\"[]\"");

    std::vector< int >expected { 1, 4, 23 };
    for ( const char *id : { "a1", "a2", "a3", "a4" } ) {
        IntArray a;
        ir->giveField(a, id);
        CHECK(equal(a, expected) );
    }

    FloatArray f1, f2;
    ir->giveField(f1, "f1");
    ir->giveField(f2, "f2");
    CHECK(equal(f1, { 1.0, -2.5 }) );
    CHECK(equal(f2, { 1.0, -2.5 }) );

    FloatMatrix m1, m2, m3;
    ir->giveField(m1, "m1");
    ir->giveField(m2, "m2");
    ir->giveField(m3, "m3");
    REQUIRE(m1.giveNumberOfRows() == 2);
    REQUIRE(m1.giveNumberOfColumns() == 3);
    CHECK(m1.at(2, 3) == 6.0);
    CHECK(equal(m2, m1) );
    CHECK(equal(m3, m1) );

    Coordinates c;
    ir->giveField(c, "c");
    CHECK(c [ 2 ] == 3.0);

    IntArray e;
    ir->giveField(e, "e");
    CHECK(e.giveSize() == 0);

    FloatMatrix em(2, 2);
    ir->giveField(em, "em");
    CHECK(em.giveNumberOfRows() == 0);
}

TEST_CASE("xml: malformed bracketed values are rejected", "[inputrecord][xml]")
{
    std::shared_ptr< XMLDataReader >reader;
    auto ir = xmlTop(reader, "a=\"[1 2\" m=\"[1 2; 3]\"");
    IntArray a;
    FloatMatrix m;
    CHECK_THROWS_AS(ir->giveField(a, "a"), RuntimeException);
    CHECK_THROWS_AS(ir->giveField(m, "m"), RuntimeException);
}
#endif
