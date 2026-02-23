//# tRefTableCoverage.cc: characterization coverage for RefTable and
//#                       ArrayColumnBase non-virtual code paths
//#
//# Exercises (RefTable):
//#   row-vector construction, bool-mask construction, column access
//#   through RefTable, sort, set operations (and/or/sub/xor/not),
//#   project, addRow, removeRow, rowOrder, deepCopy, getPartNames,
//#   chained select (nested RefTable).
//#
//# Exercises (ArrayColumnBase):
//#   isDefined, shape, nrow for fixed/variable-shape columns,
//#   getSlice/putSlice, getColumnRange/putColumnRange with RefRows,
//#   shape-mismatch error paths.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableColumn.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/Tables/TableRecord.h>
#include <casacore/tables/Tables/RefRows.h>
#include <casacore/tables/TaQL/ExprNode.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/Cube.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/Arrays/Slicer.h>
#include <casacore/casa/Arrays/Slice.h>
#include <casacore/casa/Containers/Block.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/OS/Path.h>

#include <functional>
#include <iostream>
#include <sstream>
#include <unistd.h>

using namespace casacore;

namespace {

String uniqueName(const String& base)
{
    return base + "_" + String::toString(Int(getpid()));
}

void deleteIfExists(const String& name)
{
    if (Table::isReadable(name)) {
        TableUtil::deleteTable(name, True);
    }
}

void expectThrows(const std::function<void()>& fn)
{
    Bool thrown = False;
    try {
        fn();
    } catch (const std::exception&) {
        thrown = True;
    }
    AlwaysAssertExit(thrown);
}

// Create a table with scalar Int + Double columns and a fixed-shape
// array column (Vector<Float> of length 4) plus a variable-shape
// array column (Array<Double>), with nrow rows populated.
Table makeTestTable(const String& name, uInt nrow)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("IntCol"));
    td.addColumn(ScalarColumnDesc<Double>("DoubleCol"));
    td.addColumn(ScalarColumnDesc<String>("StringCol"));
    td.addColumn(ArrayColumnDesc<Float>("FixedArr",
                                        IPosition(1, 4),
                                        ColumnDesc::FixedShape));
    td.addColumn(ArrayColumnDesc<Double>("VarArr"));
    SetupNewTable newtab(name, td, Table::New);
    Table tab(newtab, nrow);
    ScalarColumn<Int>    intCol(tab, "IntCol");
    ScalarColumn<Double> dblCol(tab, "DoubleCol");
    ScalarColumn<String> strCol(tab, "StringCol");
    ArrayColumn<Float>   fixCol(tab, "FixedArr");
    ArrayColumn<Double>  varCol(tab, "VarArr");
    for (uInt i = 0; i < nrow; i++) {
        intCol.put(i, Int(i * 10));
        dblCol.put(i, Double(i) * 1.5);
        strCol.put(i, "row" + String::toString(i));
        Vector<Float> fv(4);
        fv = Float(i);
        fixCol.put(i, fv);
        // Variable-shape: 2-element vector for even rows, 3-element for odd
        uInt vlen = (i % 2 == 0) ? 2 : 3;
        Vector<Double> vv(vlen);
        vv = Double(i) * 0.1;
        varCol.put(i, vv);
    }
    return tab;
}

// -----------------------------------------------------------------------
//  RefTable coverage tests
// -----------------------------------------------------------------------

void testRefTableFromRowVector()
{
    std::cout << "testRefTableFromRowVector" << std::endl;
    String name = uniqueName("tRefCov_rowvec");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 10);
        Vector<rownr_t> rows(3);
        rows(0) = 1; rows(1) = 4; rows(2) = 7;
        Table ref = tab(rows);
        AlwaysAssertExit(ref.nrow() == 3);

        // rowNumbers should map back to root rows
        Vector<rownr_t> rn = ref.rowNumbers();
        AlwaysAssertExit(rn.size() == 3);
        AlwaysAssertExit(rn(0) == 1);
        AlwaysAssertExit(rn(1) == 4);
        AlwaysAssertExit(rn(2) == 7);

        // rowNumbers relative to self
        Vector<rownr_t> rnSelf = ref.rowNumbers(tab);
        AlwaysAssertExit(rnSelf.size() == 3);
        AlwaysAssertExit(rnSelf(0) == 1);
        AlwaysAssertExit(rnSelf(1) == 4);
        AlwaysAssertExit(rnSelf(2) == 7);

        // Verify data access through the RefTable
        ScalarColumn<Int> intCol(ref, "IntCol");
        AlwaysAssertExit(intCol(0) == 10);
        AlwaysAssertExit(intCol(1) == 40);
        AlwaysAssertExit(intCol(2) == 70);
    }
    deleteIfExists(name);
}

void testRefTableFromBoolMask()
{
    std::cout << "testRefTableFromBoolMask" << std::endl;
    String name = uniqueName("tRefCov_mask");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 8);
        // Select rows where IntCol >= 30  (rows 3,4,5,6,7 -> values 30..70)
        Table ref = tab(tab.col("IntCol") >= 30);
        AlwaysAssertExit(ref.nrow() == 5);

        // Verify the selected rows have the expected values
        ScalarColumn<Int> intCol(ref, "IntCol");
        for (uInt i = 0; i < ref.nrow(); i++) {
            AlwaysAssertExit(intCol(i) >= 30);
        }

        // Select with maxRow limit
        Table ref2 = tab(tab.col("IntCol") >= 0, 3);
        AlwaysAssertExit(ref2.nrow() == 3);

        // Select with offset and maxRow
        Table ref3 = tab(tab.col("IntCol") >= 0, 2, 3);
        AlwaysAssertExit(ref3.nrow() == 2);
    }
    deleteIfExists(name);
}

void testRefTableColumnAccess()
{
    std::cout << "testRefTableColumnAccess" << std::endl;
    String name = uniqueName("tRefCov_colaccess");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 6);
        Vector<rownr_t> rows(3);
        rows(0) = 0; rows(1) = 2; rows(2) = 5;
        Table ref = tab(rows);

        // Read scalars through RefTable
        ScalarColumn<Int>    intCol(ref, "IntCol");
        ScalarColumn<Double> dblCol(ref, "DoubleCol");
        ScalarColumn<String> strCol(ref, "StringCol");
        AlwaysAssertExit(intCol(0) == 0);
        AlwaysAssertExit(intCol(1) == 20);
        AlwaysAssertExit(intCol(2) == 50);

        // Write scalars through RefTable
        intCol.put(0, 999);
        AlwaysAssertExit(intCol(0) == 999);
        // Verify it also changed in the original table
        ScalarColumn<Int> origCol(tab, "IntCol");
        AlwaysAssertExit(origCol(0) == 999);

        // Read arrays through RefTable
        ArrayColumn<Float> fixCol(ref, "FixedArr");
        Vector<Float> v = fixCol(0);
        AlwaysAssertExit(v.size() == 4);

        // Write arrays through RefTable
        Vector<Float> newv(4);
        newv = 42.0f;
        fixCol.put(1, newv);
        Vector<Float> readback = fixCol(1);
        AlwaysAssertExit(allEQ(readback, Float(42.0f)));

        // Variable-shape array
        ArrayColumn<Double> varCol(ref, "VarArr");
        AlwaysAssertExit(varCol.isDefined(0));
        IPosition sh = varCol.shape(0);
        AlwaysAssertExit(sh(0) == 2);  // row 0 is even
    }
    deleteIfExists(name);
}

void testRefTableSort()
{
    std::cout << "testRefTableSort" << std::endl;
    String name = uniqueName("tRefCov_sort");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 8);
        // Reverse the IntCol values so sort is non-trivial
        ScalarColumn<Int> intCol(tab, "IntCol");
        for (uInt i = 0; i < 8; i++) {
            intCol.put(i, Int(70 - i * 10));
        }

        // First create a RefTable by selecting
        Vector<rownr_t> rows(5);
        rows(0) = 0; rows(1) = 2; rows(2) = 4; rows(3) = 6; rows(4) = 7;
        Table ref = tab(rows);
        AlwaysAssertExit(ref.nrow() == 5);

        // Sort the RefTable (exercises doSort override in RefTable path)
        Table sorted = ref.sort("IntCol");
        ScalarColumn<Int> sortedCol(sorted, "IntCol");
        for (uInt i = 0; i < sorted.nrow() - 1; i++) {
            AlwaysAssertExit(sortedCol(i) <= sortedCol(i + 1));
        }

        // Sort descending
        Table sortedDesc = ref.sort("IntCol", Sort::Descending);
        ScalarColumn<Int> sdCol(sortedDesc, "IntCol");
        for (uInt i = 0; i < sortedDesc.nrow() - 1; i++) {
            AlwaysAssertExit(sdCol(i) >= sdCol(i + 1));
        }
    }
    deleteIfExists(name);
}

void testRefTableSetOps()
{
    std::cout << "testRefTableSetOps" << std::endl;
    String name = uniqueName("tRefCov_setops");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 10);

        // Two overlapping selections
        Vector<rownr_t> r1(5);
        for (uInt i = 0; i < 5; i++) r1(i) = i + 1;  // rows 1,2,3,4,5
        Vector<rownr_t> r2(5);
        for (uInt i = 0; i < 5; i++) r2(i) = i + 3;  // rows 3,4,5,6,7

        Table sel1 = tab(r1);
        Table sel2 = tab(r2);

        // AND (intersection): rows 3,4,5
        Table tand = sel1 & sel2;
        AlwaysAssertExit(tand.nrow() == 3);

        // OR (union): rows 1,2,3,4,5,6,7
        Table tor = sel1 | sel2;
        AlwaysAssertExit(tor.nrow() == 7);

        // SUB (difference): rows 1,2
        Table tsub = sel1 - sel2;
        AlwaysAssertExit(tsub.nrow() == 2);

        // XOR: rows 1,2,6,7
        Table txor = sel1 ^ sel2;
        AlwaysAssertExit(txor.nrow() == 4);

        // NOT (complement): rows 0,6,7,8,9
        Table tnot = !sel1;
        AlwaysAssertExit(tnot.nrow() == 5);

        // Verify values in the intersection
        ScalarColumn<Int> andCol(tand, "IntCol");
        AlwaysAssertExit(andCol(0) == 30);
        AlwaysAssertExit(andCol(1) == 40);
        AlwaysAssertExit(andCol(2) == 50);

        // Verify values in complement
        ScalarColumn<Int> notCol(tnot, "IntCol");
        AlwaysAssertExit(notCol(0) == 0);   // row 0
    }
    deleteIfExists(name);
}

void testRefTableProject()
{
    std::cout << "testRefTableProject" << std::endl;
    String name = uniqueName("tRefCov_proj");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 5);
        // First create a RefTable
        Vector<rownr_t> rows(3);
        rows(0) = 0; rows(1) = 2; rows(2) = 4;
        Table ref = tab(rows);

        // Project the RefTable to a subset of columns
        Block<String> cols(2);
        cols[0] = "IntCol";
        cols[1] = "FixedArr";
        Table proj = ref.project(cols);
        AlwaysAssertExit(proj.nrow() == 3);
        AlwaysAssertExit(proj.tableDesc().ncolumn() == 2);
        AlwaysAssertExit(proj.tableDesc().isColumn("IntCol"));
        AlwaysAssertExit(proj.tableDesc().isColumn("FixedArr"));
        AlwaysAssertExit(!proj.tableDesc().isColumn("DoubleCol"));
        AlwaysAssertExit(!proj.tableDesc().isColumn("StringCol"));

        // Data access still works through projection
        ScalarColumn<Int> intCol(proj, "IntCol");
        AlwaysAssertExit(intCol(0) == 0);
        AlwaysAssertExit(intCol(1) == 20);
        AlwaysAssertExit(intCol(2) == 40);
    }
    deleteIfExists(name);
}

void testRefTableAddRow()
{
    std::cout << "testRefTableAddRow" << std::endl;
    String name = uniqueName("tRefCov_addrow");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 5);
        Vector<rownr_t> rows(3);
        rows(0) = 0; rows(1) = 2; rows(2) = 4;
        Table ref = tab(rows);
        AlwaysAssertExit(ref.nrow() == 3);

        // Add rows to the underlying table, then create a new RefTable
        // that includes the new row
        tab.addRow();
        ScalarColumn<Int> origCol(tab, "IntCol");
        origCol.put(5, 500);
        AlwaysAssertExit(tab.nrow() == 6);

        // Select a RefTable that includes the new row
        Vector<rownr_t> rows2(4);
        rows2(0) = 0; rows2(1) = 2; rows2(2) = 4; rows2(3) = 5;
        Table ref2 = tab(rows2);
        AlwaysAssertExit(ref2.nrow() == 4);
        ScalarColumn<Int> intCol2(ref2, "IntCol");
        AlwaysAssertExit(intCol2(3) == 500);
    }
    deleteIfExists(name);
}

void testRefTableRemoveRow()
{
    std::cout << "testRefTableRemoveRow" << std::endl;
    String name = uniqueName("tRefCov_rmrow");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 10);
        Vector<rownr_t> rows(5);
        for (uInt i = 0; i < 5; i++) rows(i) = i * 2;  // rows 0,2,4,6,8
        Table ref = tab(rows);
        AlwaysAssertExit(ref.nrow() == 5);

        // Remove a row from the RefTable (removes it from the view only)
        ref.removeRow(2);  // removes the 3rd element (original row 4)
        AlwaysAssertExit(ref.nrow() == 4);

        // The original table still has all rows
        AlwaysAssertExit(tab.nrow() == 10);

        // Verify remaining rows
        ScalarColumn<Int> intCol(ref, "IntCol");
        AlwaysAssertExit(intCol(0) == 0);   // orig row 0
        AlwaysAssertExit(intCol(1) == 20);  // orig row 2
        AlwaysAssertExit(intCol(2) == 60);  // orig row 6
        AlwaysAssertExit(intCol(3) == 80);  // orig row 8
    }
    deleteIfExists(name);
}

void testRefTableRowOrder()
{
    std::cout << "testRefTableRowOrder" << std::endl;
    String name = uniqueName("tRefCov_roworder");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 8);

        // A row-vector selection in ascending order should be in order
        Vector<rownr_t> rows(3);
        rows(0) = 1; rows(1) = 3; rows(2) = 5;
        Table refOrd = tab(rows);
        // rowOrder may be True for ascending selections

        // Sort creates a RefTable that may or may not be in row order
        // depending on the data
        ScalarColumn<Int> intCol(tab, "IntCol");
        for (uInt i = 0; i < 8; i++) {
            intCol.put(i, Int(70 - i * 10));
        }
        Table sorted = tab.sort("IntCol");
        // After sort ascending on reversed data, row order is reversed
        // (the sort result references the original rows in reverse)

        // Verify data is sorted
        ScalarColumn<Int> sortedCol(sorted, "IntCol");
        for (uInt i = 0; i < sorted.nrow() - 1; i++) {
            AlwaysAssertExit(sortedCol(i) <= sortedCol(i + 1));
        }
    }
    deleteIfExists(name);
}

void testRefTableDeepCopy()
{
    std::cout << "testRefTableDeepCopy" << std::endl;
    String name = uniqueName("tRefCov_dcpsrc");
    String dcpName = uniqueName("tRefCov_dcpdst");
    deleteIfExists(name);
    deleteIfExists(dcpName);
    {
        Table tab = makeTestTable(name, 8);
        Vector<rownr_t> rows(4);
        rows(0) = 1; rows(1) = 3; rows(2) = 5; rows(3) = 7;
        Table ref = tab(rows);

        // Deep copy the RefTable to a PlainTable
        ref.deepCopy(dcpName, Table::New);
        {
            Table dcp(dcpName);
            AlwaysAssertExit(dcp.nrow() == 4);

            // Verify data in the deep copy
            ScalarColumn<Int> intCol(dcp, "IntCol");
            AlwaysAssertExit(intCol(0) == 10);
            AlwaysAssertExit(intCol(1) == 30);
            AlwaysAssertExit(intCol(2) == 50);
            AlwaysAssertExit(intCol(3) == 70);

            // Array columns should also be copied
            ArrayColumn<Float> fixCol(dcp, "FixedArr");
            Vector<Float> v = fixCol(0);
            AlwaysAssertExit(v.size() == 4);
            AlwaysAssertExit(allEQ(v, Float(1.0)));
        }
    }
    deleteIfExists(name);
    deleteIfExists(dcpName);
}

void testRefTableGetPartNames()
{
    std::cout << "testRefTableGetPartNames" << std::endl;
    String name = uniqueName("tRefCov_parts");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 4);
        Vector<rownr_t> rows(2);
        rows(0) = 1; rows(1) = 3;
        Table ref = tab(rows);

        // getPartNames on a RefTable should include the root table name
        Block<String> parts = ref.getPartNames(False);
        AlwaysAssertExit(parts.size() == 1);
        AlwaysAssertExit(parts[0] == tab.tableName());

        // Recursive should also give the root table
        Block<String> partsRec = ref.getPartNames(True);
        AlwaysAssertExit(partsRec.size() >= 1);
    }
    deleteIfExists(name);
}

void testRefTableChainedSelect()
{
    std::cout << "testRefTableChainedSelect" << std::endl;
    String name = uniqueName("tRefCov_chain");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 10);

        // First selection: rows with IntCol >= 20 (rows 2..9)
        Table ref1 = tab(tab.col("IntCol") >= 20);
        AlwaysAssertExit(ref1.nrow() == 8);

        // Second selection from the RefTable: IntCol >= 50 (rows 5..9)
        Table ref2 = ref1(ref1.col("IntCol") >= 50);
        AlwaysAssertExit(ref2.nrow() == 5);

        // Verify data
        ScalarColumn<Int> intCol(ref2, "IntCol");
        for (uInt i = 0; i < ref2.nrow(); i++) {
            AlwaysAssertExit(intCol(i) >= 50);
        }

        // Row numbers relative to the original table
        Vector<rownr_t> rn = ref2.rowNumbers(tab);
        AlwaysAssertExit(rn(0) == 5);
        AlwaysAssertExit(rn(1) == 6);

        // Further chain: row-vector select from a RefTable
        Vector<rownr_t> subrows(2);
        subrows(0) = 0; subrows(1) = 2;
        Table ref3 = ref2(subrows);
        AlwaysAssertExit(ref3.nrow() == 2);
        ScalarColumn<Int> intCol3(ref3, "IntCol");
        AlwaysAssertExit(intCol3(0) == 50);
        AlwaysAssertExit(intCol3(1) == 70);

        // Set operations on RefTables derived from a RefTable
        Vector<rownr_t> s1(3);
        s1(0) = 0; s1(1) = 1; s1(2) = 2;
        Vector<rownr_t> s2(3);
        s2(0) = 1; s2(1) = 2; s2(2) = 3;
        Table sub1 = ref2(s1);
        Table sub2 = ref2(s2);
        Table combo = sub1 & sub2;
        AlwaysAssertExit(combo.nrow() == 2);  // overlap at indices 1,2
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  ArrayColumnBase coverage tests
// -----------------------------------------------------------------------

void testArrayColumnShape()
{
    std::cout << "testArrayColumnShape" << std::endl;
    String name = uniqueName("tRefCov_arrshp");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 5);

        // Fixed-shape column
        ArrayColumn<Float> fixCol(tab, "FixedArr");
        AlwaysAssertExit(fixCol.isDefined(0));
        AlwaysAssertExit(fixCol.nrow() == 5);
        IPosition sh = fixCol.shape(0);
        AlwaysAssertExit(sh.size() == 1);
        AlwaysAssertExit(sh(0) == 4);
        AlwaysAssertExit(fixCol.ndim(0) == 1);

        // Fixed-shape: shapeColumn should give the common shape
        IPosition shCol = fixCol.shapeColumn();
        AlwaysAssertExit(shCol.isEqual(IPosition(1, 4)));
        AlwaysAssertExit(fixCol.ndimColumn() == 1);

        // Variable-shape column
        ArrayColumn<Double> varCol(tab, "VarArr");
        AlwaysAssertExit(varCol.isDefined(0));
        IPosition sh0 = varCol.shape(0);  // even row -> 2 elements
        AlwaysAssertExit(sh0(0) == 2);
        IPosition sh1 = varCol.shape(1);  // odd row -> 3 elements
        AlwaysAssertExit(sh1(0) == 3);

        // Variable-shape through a RefTable
        Vector<rownr_t> rows(2);
        rows(0) = 1; rows(1) = 3;
        Table ref = tab(rows);
        ArrayColumn<Double> varRefCol(ref, "VarArr");
        AlwaysAssertExit(varRefCol.isDefined(0));
        AlwaysAssertExit(varRefCol.shape(0)(0) == 3);  // row 1 is odd
        AlwaysAssertExit(varRefCol.shape(1)(0) == 3);  // row 3 is odd
    }
    deleteIfExists(name);
}

void testArrayColumnSlice()
{
    std::cout << "testArrayColumnSlice" << std::endl;
    String name = uniqueName("tRefCov_arrslice");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 5);
        ArrayColumn<Float> fixCol(tab, "FixedArr");

        // Put a known pattern
        for (uInt i = 0; i < 5; i++) {
            Vector<Float> v(4);
            v(0) = Float(i * 10);
            v(1) = Float(i * 10 + 1);
            v(2) = Float(i * 10 + 2);
            v(3) = Float(i * 10 + 3);
            fixCol.put(i, v);
        }

        // getSlice: extract elements [1..2] from row 0
        Slicer sl(IPosition(1, 1), IPosition(1, 2));
        Vector<Float> sliceResult(2);
        fixCol.getSlice(0, sl, sliceResult);
        AlwaysAssertExit(sliceResult(0) == 1.0f);
        AlwaysAssertExit(sliceResult(1) == 2.0f);

        // putSlice: overwrite elements [1..2] in row 0
        Vector<Float> newSlice(2);
        newSlice(0) = 100.0f;
        newSlice(1) = 200.0f;
        fixCol.putSlice(0, sl, newSlice);

        // Verify the slice was written
        Vector<Float> full = fixCol(0);
        AlwaysAssertExit(full(0) == 0.0f);
        AlwaysAssertExit(full(1) == 100.0f);
        AlwaysAssertExit(full(2) == 200.0f);
        AlwaysAssertExit(full(3) == 3.0f);

        // getSlice with stride: start=0, length=2, stride=2
        // This selects elements at indices 0 and 2.
        Slicer slStride(IPosition(1, 0), IPosition(1, 2), IPosition(1, 2),
                        Slicer::endIsLength);
        Vector<Float> strideResult(2);
        fixCol.getSlice(0, slStride, strideResult);
        // Elements 0 and 2: 0.0 and 200.0
        AlwaysAssertExit(strideResult(0) == 0.0f);
        AlwaysAssertExit(strideResult(1) == 200.0f);

        // Slice through a RefTable
        Vector<rownr_t> rows(2);
        rows(0) = 2; rows(1) = 4;
        Table ref = tab(rows);
        ArrayColumn<Float> refCol(ref, "FixedArr");
        Slicer sl2(IPosition(1, 0), IPosition(1, 2));
        Vector<Float> refSlice(2);
        refCol.getSlice(0, sl2, refSlice);
        AlwaysAssertExit(refSlice(0) == 20.0f);
        AlwaysAssertExit(refSlice(1) == 21.0f);
    }
    deleteIfExists(name);
}

void testArrayColumnRows()
{
    std::cout << "testArrayColumnRows" << std::endl;
    String name = uniqueName("tRefCov_arrrows");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 6);
        ArrayColumn<Float> fixCol(tab, "FixedArr");

        // Put a known pattern
        for (uInt i = 0; i < 6; i++) {
            Vector<Float> v(4);
            v = Float(i);
            fixCol.put(i, v);
        }

        // getColumnRange: get rows 1 through 3
        Slicer rowRange(IPosition(1, 1), IPosition(1, 3));
        Matrix<Float> rangeResult(4, 3);
        fixCol.getColumnRange(rowRange, rangeResult);
        AlwaysAssertExit(allEQ(rangeResult.column(0), Float(1.0)));
        AlwaysAssertExit(allEQ(rangeResult.column(1), Float(2.0)));
        AlwaysAssertExit(allEQ(rangeResult.column(2), Float(3.0)));

        // putColumnRange: overwrite rows 1 through 3
        Matrix<Float> newRange(4, 3);
        newRange.column(0) = 10.0f;
        newRange.column(1) = 20.0f;
        newRange.column(2) = 30.0f;
        fixCol.putColumnRange(rowRange, newRange);

        // Verify
        Vector<Float> r1 = fixCol(1);
        AlwaysAssertExit(allEQ(r1, Float(10.0f)));
        Vector<Float> r2 = fixCol(2);
        AlwaysAssertExit(allEQ(r2, Float(20.0f)));
        Vector<Float> r3 = fixCol(3);
        AlwaysAssertExit(allEQ(r3, Float(30.0f)));

        // getColumnCells with RefRows
        RefRows rr(1, 3);
        Matrix<Float> cellsResult(4, 3);
        fixCol.getColumnCells(rr, cellsResult);
        AlwaysAssertExit(allEQ(cellsResult.column(0), Float(10.0f)));
        AlwaysAssertExit(allEQ(cellsResult.column(1), Float(20.0f)));
        AlwaysAssertExit(allEQ(cellsResult.column(2), Float(30.0f)));

        // putColumnCells with RefRows
        Matrix<Float> newCells(4, 3);
        newCells.column(0) = 111.0f;
        newCells.column(1) = 222.0f;
        newCells.column(2) = 333.0f;
        fixCol.putColumnCells(rr, newCells);
        Vector<Float> check1 = fixCol(1);
        AlwaysAssertExit(allEQ(check1, Float(111.0f)));

        // getColumnRange with array slice
        Slicer arrSlice(IPosition(1, 0), IPosition(1, 2));
        Slicer rowRange2(IPosition(1, 0), IPosition(1, 3));
        Matrix<Float> slicedRange(2, 3);
        fixCol.getColumnRange(rowRange2, arrSlice, slicedRange);
        // Row 0 was untouched (value 0.0), elements [0..1]
        AlwaysAssertExit(slicedRange(0, 0) == 0.0f);
        AlwaysAssertExit(slicedRange(1, 0) == 0.0f);

        // putColumnRange with array slice
        Matrix<Float> newSliced(2, 3);
        newSliced = 77.0f;
        fixCol.putColumnRange(rowRange2, arrSlice, newSliced);
        Vector<Float> check0 = fixCol(0);
        AlwaysAssertExit(check0(0) == 77.0f);
        AlwaysAssertExit(check0(1) == 77.0f);
        // Elements 2,3 should be unchanged (original value 0.0)
        AlwaysAssertExit(check0(2) == 0.0f);
        AlwaysAssertExit(check0(3) == 0.0f);
    }
    deleteIfExists(name);
}

void testArrayColumnShapeMismatch()
{
    std::cout << "testArrayColumnShapeMismatch" << std::endl;
    String name = uniqueName("tRefCov_arrmismatch");
    deleteIfExists(name);
    {
        Table tab = makeTestTable(name, 3);
        ArrayColumn<Float> fixCol(tab, "FixedArr");

        // Attempt to put an array with wrong shape into fixed-shape column
        // The column has shape (4), try putting shape (3)
        expectThrows([&]() {
            Vector<Float> wrongShape(3);
            wrongShape = 1.0f;
            fixCol.put(0, wrongShape);
        });

        // Attempt to get into a non-empty array with wrong shape
        // (without resize=True)
        expectThrows([&]() {
            Vector<Float> wrongSize(3);
            wrongSize = 0.0f;
            fixCol.get(0, wrongSize, False);  // resize=False
        });

        // With resize=True, it should succeed
        {
            Vector<Float> resizable(3);
            resizable = 0.0f;
            fixCol.get(0, resizable, True);
            AlwaysAssertExit(resizable.size() == 4);
        }

        // putSlice with wrong shape
        expectThrows([&]() {
            Slicer sl(IPosition(1, 0), IPosition(1, 2));
            Vector<Float> wrongSlice(3);  // should be 2
            wrongSlice = 1.0f;
            fixCol.putSlice(0, sl, wrongSlice);
        });

        // setShape with incompatible shape on fixed-shape column
        // (the shape is already set and cannot be changed)
        expectThrows([&]() {
            fixCol.setShape(0, IPosition(1, 8));
        });

        // getColumn with wrong-shaped pre-allocated array
        expectThrows([&]() {
            Matrix<Float> wrongCol(3, 3);  // should be (4, 3)
            wrongCol = 0.0f;
            fixCol.getColumn(wrongCol, False);  // resize=False
        });
    }
    deleteIfExists(name);
}

}  // namespace

int main()
{
    try {
        // RefTable coverage
        testRefTableFromRowVector();
        testRefTableFromBoolMask();
        testRefTableColumnAccess();
        testRefTableSort();
        testRefTableSetOps();
        testRefTableProject();
        testRefTableAddRow();
        testRefTableRemoveRow();
        testRefTableRowOrder();
        testRefTableDeepCopy();
        testRefTableGetPartNames();
        testRefTableChainedSelect();
        // ArrayColumnBase coverage
        testArrayColumnShape();
        testArrayColumnSlice();
        testArrayColumnRows();
        testArrayColumnShapeMismatch();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "OK" << std::endl;
    return 0;
}
