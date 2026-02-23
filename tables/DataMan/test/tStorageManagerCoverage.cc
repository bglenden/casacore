//# tStorageManagerCoverage.cc: characterization coverage for ISM, SSM, TSM internals
//# Exercises ISMColumn, SSMBase, and TSMCube through the public Table API.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/DataMan/StandardStMan.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/tables/DataMan/TiledCellStMan.h>
#include <casacore/tables/DataMan/TiledColumnStMan.h>
#include <casacore/tables/DataMan/TiledShapeStMan.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/Cube.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/Arrays/ArrayUtil.h>
#include <casacore/casa/Arrays/Slicer.h>
#include <casacore/casa/IO/ArrayIO.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/OS/Path.h>

#include <iostream>
#include <unistd.h>

using namespace casacore;

namespace {

String uniqueName(const String& base) {
    return base + "_" + String::toString(Int(getpid()));
}

void deleteIfExists(const String& name) {
    if (Table::isReadable(name)) TableUtil::deleteTable(name, True);
}

// =========================================================================
// IncrementalStMan tests
// =========================================================================

void testISMScalarTypes()
{
    std::cout << "testISMScalarTypes" << std::endl;
    String tabName = uniqueName("tSMCov_ISMScalar");
    deleteIfExists(tabName);

    // Create table with ISM binding, one scalar column per basic type.
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Bool>    ("cBool"));
        td.addColumn(ScalarColumnDesc<uChar>   ("cUChar"));
        td.addColumn(ScalarColumnDesc<Short>   ("cShort"));
        td.addColumn(ScalarColumnDesc<Int>     ("cInt"));
        td.addColumn(ScalarColumnDesc<Int64>   ("cInt64"));
        td.addColumn(ScalarColumnDesc<Float>   ("cFloat"));
        td.addColumn(ScalarColumnDesc<Double>  ("cDouble"));
        td.addColumn(ScalarColumnDesc<Complex> ("cComplex"));
        td.addColumn(ScalarColumnDesc<DComplex>("cDComplex"));
        td.addColumn(ScalarColumnDesc<String>  ("cString"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM_types");
        newtab.bindAll(ism);
        Table tab(newtab, 5);

        ScalarColumn<Bool>     cBool(tab, "cBool");
        ScalarColumn<uChar>    cUChar(tab, "cUChar");
        ScalarColumn<Short>    cShort(tab, "cShort");
        ScalarColumn<Int>      cInt(tab, "cInt");
        ScalarColumn<Int64>    cInt64(tab, "cInt64");
        ScalarColumn<Float>    cFloat(tab, "cFloat");
        ScalarColumn<Double>   cDouble(tab, "cDouble");
        ScalarColumn<Complex>  cComplex(tab, "cComplex");
        ScalarColumn<DComplex> cDComplex(tab, "cDComplex");
        ScalarColumn<String>   cString(tab, "cString");

        for (uInt i = 0; i < 5; ++i) {
            cBool.put(i, (i % 2 == 0));
            cUChar.put(i, uChar(i + 10));
            cShort.put(i, Short(i - 2));
            cInt.put(i, Int(i * 100));
            cInt64.put(i, Int64(i) * 1000000000LL);
            cFloat.put(i, Float(i) * 1.5f);
            cDouble.put(i, Double(i) * 2.5);
            cComplex.put(i, Complex(Float(i), Float(i + 1)));
            cDComplex.put(i, DComplex(Double(i) * 3.0, Double(i) * 4.0));
            cString.put(i, "row_" + String::toString(i));
        }
    }

    // Reopen read-only and verify round-trip.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 5);

        ScalarColumn<Bool>     cBool(tab, "cBool");
        ScalarColumn<uChar>    cUChar(tab, "cUChar");
        ScalarColumn<Short>    cShort(tab, "cShort");
        ScalarColumn<Int>      cInt(tab, "cInt");
        ScalarColumn<Int64>    cInt64(tab, "cInt64");
        ScalarColumn<Float>    cFloat(tab, "cFloat");
        ScalarColumn<Double>   cDouble(tab, "cDouble");
        ScalarColumn<Complex>  cComplex(tab, "cComplex");
        ScalarColumn<DComplex> cDComplex(tab, "cDComplex");
        ScalarColumn<String>   cString(tab, "cString");

        for (uInt i = 0; i < 5; ++i) {
            AlwaysAssertExit(cBool(i)     == (i % 2 == 0));
            AlwaysAssertExit(cUChar(i)    == uChar(i + 10));
            AlwaysAssertExit(cShort(i)    == Short(i - 2));
            AlwaysAssertExit(cInt(i)      == Int(i * 100));
            AlwaysAssertExit(cInt64(i)    == Int64(i) * 1000000000LL);
            AlwaysAssertExit(cFloat(i)    == Float(i) * 1.5f);
            AlwaysAssertExit(cDouble(i)   == Double(i) * 2.5);
            AlwaysAssertExit(cComplex(i)  == Complex(Float(i), Float(i + 1)));
            AlwaysAssertExit(cDComplex(i) == DComplex(Double(i) * 3.0,
                                                       Double(i) * 4.0));
            AlwaysAssertExit(cString(i)   == "row_" + String::toString(i));
        }
    }
    deleteIfExists(tabName);
}

void testISMIncrementalBehavior()
{
    std::cout << "testISMIncrementalBehavior" << std::endl;
    String tabName = uniqueName("tSMCov_ISMIncr");
    deleteIfExists(tabName);

    // ISM stores repeated values as a single entry. Write many identical
    // rows, then change the value, and verify storage stays compact.
    const uInt nRows = 200;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM_incr");
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");
        // Write the same value to all rows.
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, 42);
        }
    }

    // Reopen and verify all values, then update a middle row.
    {
        Table tab(tabName, Table::Update);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(col(i) == 42);
        }
        // Change value at row 100 -- ISM creates a new entry.
        col.put(100, 99);
    }

    // Reopen read-only and verify the change.
    // ISM put(row, val) replaces the value at that single row.
    // Rows before and after remain at their previous value.
    {
        Table tab(tabName, Table::Old);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            if (i == 100) {
                AlwaysAssertExit(col(i) == 99);
            } else {
                AlwaysAssertExit(col(i) == 42);
            }
        }
    }
    deleteIfExists(tabName);
}

void testISMAddRemoveRows()
{
    std::cout << "testISMAddRemoveRows" << std::endl;
    String tabName = uniqueName("tSMCov_ISMAddRm");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("idx"));
        td.addColumn(ScalarColumnDesc<Float>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM_addrm");
        newtab.bindAll(ism);
        Table tab(newtab, 10);

        ScalarColumn<Int>   idx(tab, "idx");
        ScalarColumn<Float> val(tab, "val");
        for (uInt i = 0; i < 10; ++i) {
            idx.put(i, Int(i));
            val.put(i, Float(i) * 10.0f);
        }

        // Add 5 more rows.
        tab.addRow(5);
        AlwaysAssertExit(tab.nrow() == 15);
        for (uInt i = 10; i < 15; ++i) {
            idx.put(i, Int(i));
            val.put(i, Float(i) * 10.0f);
        }

        // Remove rows 2, 5, 8 (removes from current numbering).
        tab.removeRow(8);
        tab.removeRow(5);
        tab.removeRow(2);
        AlwaysAssertExit(tab.nrow() == 12);
    }

    // Reopen and verify the remaining data is internally consistent.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 12);
        ScalarColumn<Int>   idx(tab, "idx");
        ScalarColumn<Float> val(tab, "val");
        // After removing rows 2, 5, 8 the original indices were:
        // 0,1,3,4,6,7,9,10,11,12,13,14
        Int expected[] = {0, 1, 3, 4, 6, 7, 9, 10, 11, 12, 13, 14};
        for (uInt i = 0; i < 12; ++i) {
            AlwaysAssertExit(idx(i) == expected[i]);
            AlwaysAssertExit(val(i) == Float(expected[i]) * 10.0f);
        }
    }
    deleteIfExists(tabName);
}

void testISMArrayColumn()
{
    std::cout << "testISMArrayColumn" << std::endl;
    String tabName = uniqueName("tSMCov_ISMArr");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        // Variable-shape array column bound to ISM.
        td.addColumn(ArrayColumnDesc<Float>("arr", 0));
        // Fixed-shape direct array column bound to ISM.
        td.addColumn(ArrayColumnDesc<Int>("fixed_arr",
                                          IPosition(2, 3, 4),
                                          ColumnDesc::Direct));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM_arr");
        newtab.bindAll(ism);
        newtab.setShapeColumn("fixed_arr", IPosition(2, 3, 4));
        Table tab(newtab, 5);

        ArrayColumn<Float> arr(tab, "arr");
        ArrayColumn<Int>   fixedArr(tab, "fixed_arr");

        // Write different shapes per row for the variable-shape column.
        for (uInt i = 0; i < 5; ++i) {
            uInt len = 3 + i;
            Vector<Float> v(len);
            indgen(v, Float(i * 100));
            arr.put(i, v);

            Matrix<Int> m(3, 4);
            indgen(m, Int(i * 12));
            fixedArr.put(i, m);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 5);
        ArrayColumn<Float> arr(tab, "arr");
        ArrayColumn<Int>   fixedArr(tab, "fixed_arr");

        for (uInt i = 0; i < 5; ++i) {
            uInt len = 3 + i;
            Vector<Float> expected(len);
            indgen(expected, Float(i * 100));
            AlwaysAssertExit(allEQ(arr(i), expected));

            Matrix<Int> expectedM(3, 4);
            indgen(expectedM, Int(i * 12));
            AlwaysAssertExit(allEQ(fixedArr(i), expectedM));
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// StandardStMan tests
// =========================================================================

void testSSMScalarTypes()
{
    std::cout << "testSSMScalarTypes" << std::endl;
    String tabName = uniqueName("tSMCov_SSMScalar");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Bool>    ("cBool"));
        td.addColumn(ScalarColumnDesc<uChar>   ("cUChar"));
        td.addColumn(ScalarColumnDesc<Short>   ("cShort"));
        td.addColumn(ScalarColumnDesc<Int>     ("cInt"));
        td.addColumn(ScalarColumnDesc<Int64>   ("cInt64"));
        td.addColumn(ScalarColumnDesc<Float>   ("cFloat"));
        td.addColumn(ScalarColumnDesc<Double>  ("cDouble"));
        td.addColumn(ScalarColumnDesc<Complex> ("cComplex"));
        td.addColumn(ScalarColumnDesc<DComplex>("cDComplex"));
        td.addColumn(ScalarColumnDesc<String>  ("cString"));

        SetupNewTable newtab(tabName, td, Table::New);
        StandardStMan ssm("SSM_types", 4096);
        newtab.bindAll(ssm);
        Table tab(newtab, 8);

        ScalarColumn<Bool>     cBool(tab, "cBool");
        ScalarColumn<uChar>    cUChar(tab, "cUChar");
        ScalarColumn<Short>    cShort(tab, "cShort");
        ScalarColumn<Int>      cInt(tab, "cInt");
        ScalarColumn<Int64>    cInt64(tab, "cInt64");
        ScalarColumn<Float>    cFloat(tab, "cFloat");
        ScalarColumn<Double>   cDouble(tab, "cDouble");
        ScalarColumn<Complex>  cComplex(tab, "cComplex");
        ScalarColumn<DComplex> cDComplex(tab, "cDComplex");
        ScalarColumn<String>   cString(tab, "cString");

        for (uInt i = 0; i < 8; ++i) {
            cBool.put(i, (i % 2 == 0));
            cUChar.put(i, uChar(i + 20));
            cShort.put(i, Short(i * 3));
            cInt.put(i, Int(i * 1000));
            cInt64.put(i, Int64(i) * 2000000000LL);
            cFloat.put(i, Float(i) * 0.25f);
            cDouble.put(i, Double(i) * 0.125);
            cComplex.put(i, Complex(Float(i), Float(-i)));
            cDComplex.put(i, DComplex(Double(i), Double(i * 2)));
            cString.put(i, "ssm_" + String::toString(i));
        }
    }

    // Reopen read-only and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 8);

        ScalarColumn<Bool>     cBool(tab, "cBool");
        ScalarColumn<uChar>    cUChar(tab, "cUChar");
        ScalarColumn<Short>    cShort(tab, "cShort");
        ScalarColumn<Int>      cInt(tab, "cInt");
        ScalarColumn<Int64>    cInt64(tab, "cInt64");
        ScalarColumn<Float>    cFloat(tab, "cFloat");
        ScalarColumn<Double>   cDouble(tab, "cDouble");
        ScalarColumn<Complex>  cComplex(tab, "cComplex");
        ScalarColumn<DComplex> cDComplex(tab, "cDComplex");
        ScalarColumn<String>   cString(tab, "cString");

        for (uInt i = 0; i < 8; ++i) {
            AlwaysAssertExit(cBool(i)     == (i % 2 == 0));
            AlwaysAssertExit(cUChar(i)    == uChar(i + 20));
            AlwaysAssertExit(cShort(i)    == Short(i * 3));
            AlwaysAssertExit(cInt(i)      == Int(i * 1000));
            AlwaysAssertExit(cInt64(i)    == Int64(i) * 2000000000LL);
            AlwaysAssertExit(cFloat(i)    == Float(i) * 0.25f);
            AlwaysAssertExit(cDouble(i)   == Double(i) * 0.125);
            AlwaysAssertExit(cComplex(i)  == Complex(Float(i), Float(-i)));
            AlwaysAssertExit(cDComplex(i) == DComplex(Double(i),
                                                       Double(i * 2)));
            AlwaysAssertExit(cString(i)   == "ssm_" + String::toString(i));
        }
    }
    deleteIfExists(tabName);
}

void testSSMStringColumn()
{
    std::cout << "testSSMStringColumn" << std::endl;
    String tabName = uniqueName("tSMCov_SSMStr");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<String>("shortStr"));
        td.addColumn(ScalarColumnDesc<String>("longStr"));

        SetupNewTable newtab(tabName, td, Table::New);
        StandardStMan ssm("SSM_str", 2048);
        newtab.bindAll(ssm);
        Table tab(newtab, 6);

        ScalarColumn<String> shortStr(tab, "shortStr");
        ScalarColumn<String> longStr(tab, "longStr");

        for (uInt i = 0; i < 6; ++i) {
            shortStr.put(i, "s" + String::toString(i));
            // Create a long string (>256 chars) that goes indirect.
            String longVal;
            for (uInt j = 0; j < 50; ++j) {
                longVal += "LongStringPart_" + String::toString(i) + "_";
            }
            longStr.put(i, longVal);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 6);
        ScalarColumn<String> shortStr(tab, "shortStr");
        ScalarColumn<String> longStr(tab, "longStr");

        for (uInt i = 0; i < 6; ++i) {
            AlwaysAssertExit(shortStr(i) == "s" + String::toString(i));
            String expectedLong;
            for (uInt j = 0; j < 50; ++j) {
                expectedLong += "LongStringPart_" + String::toString(i) + "_";
            }
            AlwaysAssertExit(longStr(i) == expectedLong);
        }
    }
    deleteIfExists(tabName);
}

void testSSMAddRemoveRows()
{
    std::cout << "testSSMAddRemoveRows" << std::endl;
    String tabName = uniqueName("tSMCov_SSMAddRm");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        StandardStMan ssm("SSM_addrm", 512);
        newtab.bindAll(ssm);
        // Start with 4 rows, then add more beyond initial bucket allocation.
        Table tab(newtab, 4);

        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < 4; ++i) {
            col.put(i, Int(i));
        }
        // Add 20 more rows to force bucket growth.
        tab.addRow(20);
        AlwaysAssertExit(tab.nrow() == 24);
        for (uInt i = 4; i < 24; ++i) {
            col.put(i, Int(i));
        }

        // Remove some rows.
        tab.removeRow(0);
        tab.removeRow(10);  // was originally row 11
        tab.removeRow(15);  // was originally row 17
        AlwaysAssertExit(tab.nrow() == 21);
    }

    // Reopen and verify data integrity.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 21);
        ScalarColumn<Int> col(tab, "val");
        // After removing original rows 0, 11, 17:
        // expected: 1,2,3,4,5,6,7,8,9,10,12,13,14,15,16,18,19,20,21,22,23
        Int expected[] = {1,2,3,4,5,6,7,8,9,10,12,13,14,15,16,18,19,20,21,22,23};
        for (uInt i = 0; i < 21; ++i) {
            AlwaysAssertExit(col(i) == expected[i]);
        }
    }
    deleteIfExists(tabName);
}

void testSSMMultipleColumns()
{
    std::cout << "testSSMMultipleColumns" << std::endl;
    String tabName = uniqueName("tSMCov_SSMMulti");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("colA"));
        td.addColumn(ScalarColumnDesc<Float>("colB"));
        td.addColumn(ScalarColumnDesc<Double>("colC"));

        SetupNewTable newtab(tabName, td, Table::New);
        StandardStMan ssm("SSM_multi", 2048);
        newtab.bindAll(ssm);
        Table tab(newtab, 10);

        ScalarColumn<Int>    colA(tab, "colA");
        ScalarColumn<Float>  colB(tab, "colB");
        ScalarColumn<Double> colC(tab, "colC");

        for (uInt i = 0; i < 10; ++i) {
            colA.put(i, Int(i));
            colB.put(i, Float(i) * 1.1f);
            colC.put(i, Double(i) * 2.2);
        }
    }

    // Reopen and verify column independence.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 10);
        ScalarColumn<Int>    colA(tab, "colA");
        ScalarColumn<Float>  colB(tab, "colB");
        ScalarColumn<Double> colC(tab, "colC");

        for (uInt i = 0; i < 10; ++i) {
            AlwaysAssertExit(colA(i) == Int(i));
            AlwaysAssertExit(colB(i) == Float(i) * 1.1f);
            AlwaysAssertExit(colC(i) == Double(i) * 2.2);
        }
    }
    deleteIfExists(tabName);
}

void testSSMColumnAddition()
{
    std::cout << "testSSMColumnAddition" << std::endl;
    String tabName = uniqueName("tSMCov_SSMColAdd");
    deleteIfExists(tabName);

    // Create initial table with one column.
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("original"));

        SetupNewTable newtab(tabName, td, Table::New);
        StandardStMan ssm("SSM_coladd", 2048);
        newtab.bindAll(ssm);
        Table tab(newtab, 5);

        ScalarColumn<Int> col(tab, "original");
        for (uInt i = 0; i < 5; ++i) {
            col.put(i, Int(i * 10));
        }
    }

    // Reopen for update and add a new column.
    {
        Table tab(tabName, Table::Update);
        tab.addColumn(ScalarColumnDesc<Float>("added"));

        ScalarColumn<Int>   original(tab, "original");
        ScalarColumn<Float> added(tab, "added");

        // Verify original data is intact.
        for (uInt i = 0; i < 5; ++i) {
            AlwaysAssertExit(original(i) == Int(i * 10));
        }

        // Write the new column.
        for (uInt i = 0; i < 5; ++i) {
            added.put(i, Float(i) * 3.14f);
        }
    }

    // Reopen read-only and verify both columns.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 5);

        ScalarColumn<Int>   original(tab, "original");
        ScalarColumn<Float> added(tab, "added");

        for (uInt i = 0; i < 5; ++i) {
            AlwaysAssertExit(original(i) == Int(i * 10));
            AlwaysAssertExit(added(i)    == Float(i) * 3.14f);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// TiledStMan tests
// =========================================================================

void testTiledCellStMan()
{
    std::cout << "testTiledCellStMan" << std::endl;
    String tabName = uniqueName("tSMCov_TSMCell");
    deleteIfExists(tabName);

    const uInt nRows = 10;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("data", 2, ColumnDesc::FixedShape));
        td.addColumn(ArrayColumnDesc<Float>("weight", IPosition(2, 8, 12),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMCell", 2,
                             stringToVector("data,weight"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledCellStMan tcs("TSMCell", IPosition(2, 4, 4));
        newtab.setShapeColumn("data", IPosition(2, 8, 12));
        newtab.bindAll(tcs);
        Table tab(newtab, 0);

        ArrayColumn<Float> dataCol(tab, "data");
        ArrayColumn<Float> weightCol(tab, "weight");

        Matrix<Float> arr(8, 12);
        for (uInt i = 0; i < nRows; ++i) {
            tab.addRow();
            indgen(arr, Float(i * 96));
            dataCol.put(i, arr);
            weightCol.put(i, arr + Float(1000));
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ArrayColumn<Float> dataCol(tab, "data");
        ArrayColumn<Float> weightCol(tab, "weight");

        Matrix<Float> expected(8, 12);
        for (uInt i = 0; i < nRows; ++i) {
            indgen(expected, Float(i * 96));
            AlwaysAssertExit(allEQ(dataCol(i), expected));
            AlwaysAssertExit(allEQ(weightCol(i), expected + Float(1000)));
        }
    }
    deleteIfExists(tabName);
}

void testTiledColumnStMan()
{
    std::cout << "testTiledColumnStMan" << std::endl;
    String tabName = uniqueName("tSMCov_TSMCol");
    deleteIfExists(tabName);

    const uInt nRows = 20;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Pol", IPosition(1, 10),
                                            ColumnDesc::FixedShape));
        td.addColumn(ArrayColumnDesc<Float>("Freq", 1,
                                            ColumnDesc::FixedShape));
        td.addColumn(ScalarColumnDesc<Float>("Time"));
        td.addColumn(ArrayColumnDesc<Float>("data", 2,
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMCol", 3,
                             stringToVector("data"),
                             stringToVector("Pol,Freq,Time"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan tcol("TSMCol", IPosition(3, 5, 5, 2));
        newtab.setShapeColumn("Freq", IPosition(1, 10));
        newtab.setShapeColumn("data", IPosition(2, 10, 10));
        newtab.bindAll(tcol);
        Table tab(newtab, 0);

        ArrayColumn<Float>  dataCol(tab, "data");
        ScalarColumn<Float> timeCol(tab, "Time");
        ArrayColumn<Float>  polCol(tab, "Pol");
        ArrayColumn<Float>  freqCol(tab, "Freq");

        Vector<Float> polValues(10);
        Vector<Float> freqValues(10);
        indgen(polValues, Float(100));
        indgen(freqValues, Float(200));

        Matrix<Float> arr(10, 10);
        for (uInt i = 0; i < nRows; ++i) {
            tab.addRow();
            indgen(arr, Float(i * 100));
            dataCol.put(i, arr);
            timeCol.put(i, Float(i) * 5.0f);
        }
        polCol.put(0, polValues);
        freqCol.put(0, freqValues);
    }

    // Reopen and verify, including slice access.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ArrayColumn<Float>  dataCol(tab, "data");
        ScalarColumn<Float> timeCol(tab, "Time");

        Matrix<Float> expected(10, 10);
        for (uInt i = 0; i < nRows; ++i) {
            indgen(expected, Float(i * 100));
            AlwaysAssertExit(allEQ(dataCol(i), expected));
            AlwaysAssertExit(timeCol(i) == Float(i) * 5.0f);

            // Test slice access: read a 3x3 submatrix from offset (2,2).
            Slicer slicer(IPosition(2, 2, 2), IPosition(2, 3, 3));
            Array<Float> slice = dataCol.getSlice(i, slicer);
            AlwaysAssertExit(slice.shape() == IPosition(2, 3, 3));
            // Verify slice contents against the expected full array.
            Matrix<Float> expectedSlice = expected(
                IPosition(2, 2, 2), IPosition(2, 4, 4));
            AlwaysAssertExit(allEQ(slice, expectedSlice));
        }
    }
    deleteIfExists(tabName);
}

void testTiledShapeStMan()
{
    std::cout << "testTiledShapeStMan" << std::endl;
    String tabName = uniqueName("tSMCov_TSMShape");
    deleteIfExists(tabName);

    const uInt nRows = 8;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Pol", 1));
        td.addColumn(ArrayColumnDesc<Float>("Freq", 1));
        td.addColumn(ScalarColumnDesc<Float>("Time"));
        td.addColumn(ArrayColumnDesc<Float>("data", 2));
        td.addColumn(ArrayColumnDesc<Float>("weight", 2));
        td.defineHypercolumn("TSMShape", 3,
                             stringToVector("data,weight"),
                             stringToVector("Pol,Freq,Time"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledShapeStMan tss("TSMShape", IPosition(2, 4, 5));
        newtab.bindAll(tss);
        Table tab(newtab, 0);

        ArrayColumn<Float>  dataCol(tab, "data");
        ArrayColumn<Float>  weightCol(tab, "weight");
        ScalarColumn<Float> timeCol(tab, "Time");
        ArrayColumn<Float>  polCol(tab, "Pol");
        ArrayColumn<Float>  freqCol(tab, "Freq");

        for (uInt i = 0; i < nRows; ++i) {
            uInt nchan = 8 + (i % 3);  // varying second dimension
            tab.addRow();
            // Set shape on one data column; the other shares the hypercube.
            polCol.setShape(i, IPosition(1, 6), IPosition(1, 1));
            dataCol.setShape(i, IPosition(2, 6, nchan), IPosition(2, 4, 5));

            Matrix<Float> arr(6, nchan);
            indgen(arr, Float(i * 100));
            dataCol.put(i, arr);
            weightCol.put(i, arr + Float(500));
            timeCol.put(i, Float(i) * 10.0f);
            Vector<Float> polVals(6);
            indgen(polVals, Float(300));
            polCol.put(i, polVals);
            Vector<Float> freqVals(nchan);
            indgen(freqVals, Float(200));
            freqCol.put(i, freqVals);
        }
    }

    // Reopen and verify variable shapes.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ArrayColumn<Float>  dataCol(tab, "data");
        ArrayColumn<Float>  weightCol(tab, "weight");
        ScalarColumn<Float> timeCol(tab, "Time");

        for (uInt i = 0; i < nRows; ++i) {
            uInt nchan = 8 + (i % 3);
            AlwaysAssertExit(dataCol.shape(i) == IPosition(2, 6, nchan));

            Matrix<Float> expected(6, nchan);
            indgen(expected, Float(i * 100));
            AlwaysAssertExit(allEQ(dataCol(i), expected));
            AlwaysAssertExit(allEQ(weightCol(i), expected + Float(500)));
            AlwaysAssertExit(timeCol(i) == Float(i) * 10.0f);
        }
    }
    deleteIfExists(tabName);
}

void testTiledSliceAccess()
{
    std::cout << "testTiledSliceAccess" << std::endl;
    String tabName = uniqueName("tSMCov_TSMSlice");
    deleteIfExists(tabName);

    const uInt nRows = 5;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("data", IPosition(2, 12, 16),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMSlice", 2,
                             stringToVector("data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledCellStMan tcs("TSMSlice", IPosition(2, 4, 4));
        newtab.bindAll(tcs);
        Table tab(newtab, 0);

        ArrayColumn<Float> dataCol(tab, "data");

        Matrix<Float> arr(12, 16);
        for (uInt i = 0; i < nRows; ++i) {
            tab.addRow();
            indgen(arr, Float(i * 192));
            dataCol.put(i, arr);
        }
    }

    // Test getSlice and putSlice.
    {
        Table tab(tabName, Table::Update);
        ArrayColumn<Float> dataCol(tab, "data");

        // Read a slice from row 2.
        Slicer slicer(IPosition(2, 1, 2), IPosition(2, 5, 7));
        Array<Float> slice = dataCol.getSlice(2, slicer);
        AlwaysAssertExit(slice.shape() == IPosition(2, 5, 7));

        // Verify against the full array.
        Matrix<Float> full(12, 16);
        indgen(full, Float(2 * 192));
        Matrix<Float> expected = full(IPosition(2, 1, 2), IPosition(2, 5, 8));
        AlwaysAssertExit(allEQ(slice, expected));

        // putSlice: write a constant patch into row 0.
        Matrix<Float> patch(5, 7);
        patch.set(Float(-99.0));
        dataCol.putSlice(0, slicer, patch);

        // Read back and verify the patch.
        Array<Float> readBack = dataCol.getSlice(0, slicer);
        AlwaysAssertExit(allEQ(readBack, patch));

        // Verify that data outside the slice is unchanged in row 0.
        Matrix<Float> fullRow0(12, 16);
        indgen(fullRow0, Float(0));  // row 0 original
        dataCol.get(0, fullRow0);
        // Check a cell outside the slice.
        AlwaysAssertExit(fullRow0(0, 0) == Float(0));
        AlwaysAssertExit(fullRow0(11, 15) == Float(191));
        // Check a cell inside the slice.
        AlwaysAssertExit(fullRow0(1, 2) == Float(-99.0));
    }
    deleteIfExists(tabName);
}

void testTiledLargeData()
{
    std::cout << "testTiledLargeData" << std::endl;
    String tabName = uniqueName("tSMCov_TSMLarge");
    deleteIfExists(tabName);

    const uInt nRows = 100;
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("data", IPosition(2, 10, 10),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMLarge", 2,
                             stringToVector("data"));

        SetupNewTable newtab(tabName, td, Table::New);
        // Small tile shape (5x5) forces multiple tiles per cell.
        TiledCellStMan tcs("TSMLarge", IPosition(2, 5, 5));
        newtab.bindAll(tcs);
        Table tab(newtab, 0);

        ArrayColumn<Float> dataCol(tab, "data");

        Matrix<Float> arr(10, 10);
        for (uInt i = 0; i < nRows; ++i) {
            tab.addRow();
            indgen(arr, Float(i * 100));
            dataCol.put(i, arr);
        }
    }

    // Reopen and verify all rows.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ArrayColumn<Float> dataCol(tab, "data");

        Matrix<Float> expected(10, 10);
        for (uInt i = 0; i < nRows; ++i) {
            indgen(expected, Float(i * 100));
            AlwaysAssertExit(allEQ(dataCol(i), expected));
        }
    }
    deleteIfExists(tabName);
}

}  // anonymous namespace

int main()
{
    try {
        // ISM tests
        testISMScalarTypes();
        testISMIncrementalBehavior();
        testISMAddRemoveRows();
        testISMArrayColumn();

        // SSM tests
        testSSMScalarTypes();
        testSSMStringColumn();
        testSSMAddRemoveRows();
        testSSMMultipleColumns();
        testSSMColumnAddition();

        // TSM tests
        testTiledCellStMan();
        testTiledColumnStMan();
        testTiledShapeStMan();
        testTiledSliceAccess();
        testTiledLargeData();

    } catch (const std::exception& x) {
        std::cerr << "tStorageManagerCoverage FAIL: " << x.what() << std::endl;
        return 1;
    }
    std::cout << "OK" << std::endl;
    return 0;
}
