//# tBaseTableCoverage.cc: characterization coverage for BaseTable non-virtual paths
//#
//# Exercises: construction, markForDelete/unmark, openedForWrite, rename,
//#            copy, deepCopy, select, project, sort, set operations,
//#            makeIterator, showStructure, checkRemoveColumn, checkRowNumber,
//#            getPartNames, addColumns, makeAbsoluteName, row removal paths.

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
#include <casacore/tables/Tables/TableIter.h>
#include <casacore/tables/Tables/MemoryTable.h>
#include <casacore/tables/Tables/TableInfo.h>
#include <casacore/tables/TaQL/ExprNode.h>
#include <casacore/casa/Arrays/Vector.h>
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

// Create a simple table with scalar Int and Double columns and nrow rows.
Table makeSimpleTable(const String& name, uInt nrow)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("IntCol"));
    td.addColumn(ScalarColumnDesc<Double>("DoubleCol"));
    td.addColumn(ScalarColumnDesc<String>("StringCol"));
    SetupNewTable newtab(name, td, Table::New);
    Table tab(newtab, nrow);
    ScalarColumn<Int> intCol(tab, "IntCol");
    ScalarColumn<Double> dblCol(tab, "DoubleCol");
    ScalarColumn<String> strCol(tab, "StringCol");
    for (uInt i = 0; i < nrow; i++) {
        intCol.put(i, Int(i * 10));
        dblCol.put(i, Double(i) * 1.5);
        strCol.put(i, "row" + String::toString(i));
    }
    return tab;
}

void testConstruction()
{
    std::cout << "testConstruction" << std::endl;
    // Memory table (no disk)
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("col1"));
        SetupNewTable newtab("", td, Table::New);
        Table tab(newtab, Table::Memory, 0);
        AlwaysAssertExit(tab.nrow() == 0);
        AlwaysAssertExit(!tab.tableName().empty());
    }
    // Named table with Scratch option
    {
        String name = uniqueName("tBaseTab_scratch");
        deleteIfExists(name);
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("col1"));
        SetupNewTable newtab(name, td, Table::Scratch);
        Table tab(newtab, 5);
        AlwaysAssertExit(tab.nrow() == 5);
        AlwaysAssertExit(tab.isMarkedForDelete());
    }
}

void testOpenedForWrite()
{
    std::cout << "testOpenedForWrite" << std::endl;
    String name = uniqueName("tBaseTab_write");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);
        AlwaysAssertExit(tab.isWritable());
    }
    // Open read-only
    {
        Table tab(name, Table::Old);
        AlwaysAssertExit(!tab.isWritable());
    }
    deleteIfExists(name);
}

void testMarkForDelete()
{
    std::cout << "testMarkForDelete" << std::endl;
    String name = uniqueName("tBaseTab_mark");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);
        AlwaysAssertExit(!tab.isMarkedForDelete());
        tab.markForDelete();
        AlwaysAssertExit(tab.isMarkedForDelete());
        tab.unmarkForDelete();
        AlwaysAssertExit(!tab.isMarkedForDelete());
    }
    // Table should still exist since we unmarked
    AlwaysAssertExit(Table::isReadable(name));
    deleteIfExists(name);
}

void testRename()
{
    std::cout << "testRename" << std::endl;
    String name1 = uniqueName("tBaseTab_ren1");
    String name2 = uniqueName("tBaseTab_ren2");
    deleteIfExists(name1);
    deleteIfExists(name2);
    {
        Table tab = makeSimpleTable(name1, 3);
        tab.rename(name2, Table::New);
        AlwaysAssertExit(tab.tableName() == Path(name2).absoluteName());
    }
    AlwaysAssertExit(!Table::isReadable(name1));
    AlwaysAssertExit(Table::isReadable(name2));

    // NewNoReplace should throw if target exists
    {
        String name3 = uniqueName("tBaseTab_ren3");
        deleteIfExists(name3);
        {
            Table tab = makeSimpleTable(name3, 1);
            expectThrows([&]() { tab.rename(name2, Table::NewNoReplace); });
        }
        deleteIfExists(name3);
    }
    deleteIfExists(name2);
}

void testCopyAndDeepCopy()
{
    std::cout << "testCopyAndDeepCopy" << std::endl;
    String srcName = uniqueName("tBaseTab_src");
    String cpyName = uniqueName("tBaseTab_cpy");
    String dcpName = uniqueName("tBaseTab_dcp");
    deleteIfExists(srcName);
    deleteIfExists(cpyName);
    deleteIfExists(dcpName);
    {
        Table src = makeSimpleTable(srcName, 5);

        // Shallow copy
        src.copy(cpyName, Table::New);
        {
            Table cpy(cpyName);
            AlwaysAssertExit(cpy.nrow() == 5);
        }

        // Deep copy
        src.deepCopy(dcpName, Table::New);
        {
            Table dcp(dcpName);
            AlwaysAssertExit(dcp.nrow() == 5);
        }

        // Deep copy with noRows (valueCopy must be True to get trueDeepCopy path)
        String dcpNoRows = uniqueName("tBaseTab_dcpnr");
        deleteIfExists(dcpNoRows);
        src.deepCopy(dcpNoRows, Table::New, True,
                      Table::AipsrcEndian, True);  // valueCopy=True, noRows=True
        {
            Table dnr(dcpNoRows);
            AlwaysAssertExit(dnr.nrow() == 0);
        }
        deleteIfExists(dcpNoRows);
    }
    deleteIfExists(srcName);
    deleteIfExists(cpyName);
    deleteIfExists(dcpName);
}

void testSelectRows()
{
    std::cout << "testSelectRows" << std::endl;
    String name = uniqueName("tBaseTab_sel");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 10);

        // Select by expression
        Table sel = tab(tab.col("IntCol") > 40);
        AlwaysAssertExit(sel.nrow() == 5);  // rows 5,6,7,8,9

        // Select with maxRow
        Table sel2 = tab(tab.col("IntCol") >= 0, 3);
        AlwaysAssertExit(sel2.nrow() == 3);

        // Select with offset
        Table sel3 = tab(tab.col("IntCol") >= 0, 2, 5);
        AlwaysAssertExit(sel3.nrow() == 2);

        // Select by row numbers
        Vector<rownr_t> rows(3);
        rows(0) = 1; rows(1) = 3; rows(2) = 7;
        Table sel4 = tab(rows);
        AlwaysAssertExit(sel4.nrow() == 3);

        // Select all (maxRow=0, offset=0) returns the table itself
        Table sel5 = tab(tab.col("IntCol") >= 0);
        AlwaysAssertExit(sel5.nrow() == 10);
    }
    deleteIfExists(name);
}

void testProject()
{
    std::cout << "testProject" << std::endl;
    String name = uniqueName("tBaseTab_proj");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);

        // Project to subset of columns
        Block<String> cols(2);
        cols[0] = "IntCol";
        cols[1] = "StringCol";
        Table proj = tab.project(cols);
        AlwaysAssertExit(proj.nrow() == 5);
        AlwaysAssertExit(proj.tableDesc().ncolumn() == 2);
        AlwaysAssertExit(proj.tableDesc().isColumn("IntCol"));
        AlwaysAssertExit(proj.tableDesc().isColumn("StringCol"));
        AlwaysAssertExit(!proj.tableDesc().isColumn("DoubleCol"));
    }
    deleteIfExists(name);
}

void testSort()
{
    std::cout << "testSort" << std::endl;
    String name = uniqueName("tBaseTab_sort");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);
        // Reverse the IntCol values so sort is non-trivial
        ScalarColumn<Int> intCol(tab, "IntCol");
        for (uInt i = 0; i < 5; i++) {
            intCol.put(i, Int(40 - i * 10));
        }

        // Sort ascending
        Table sorted = tab.sort("IntCol");
        ScalarColumn<Int> sortedCol(sorted, "IntCol");
        for (uInt i = 0; i < 4; i++) {
            AlwaysAssertExit(sortedCol(i) <= sortedCol(i + 1));
        }

        // Sort descending
        Table sortedDesc = tab.sort("IntCol", Sort::Descending);
        ScalarColumn<Int> sdCol(sortedDesc, "IntCol");
        for (uInt i = 0; i < 4; i++) {
            AlwaysAssertExit(sdCol(i) >= sdCol(i + 1));
        }
    }
    deleteIfExists(name);
}

void testSetOperations()
{
    std::cout << "testSetOperations" << std::endl;
    String name = uniqueName("tBaseTab_setop");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 10);

        // Create two overlapping selections using row number vectors
        Vector<rownr_t> r1(5);
        for (uInt i = 0; i < 5; i++) r1(i) = i + 2;  // rows 2,3,4,5,6
        Vector<rownr_t> r2(5);
        for (uInt i = 0; i < 5; i++) r2(i) = i + 4;  // rows 4,5,6,7,8

        Table sel1 = tab(r1);
        Table sel2 = tab(r2);
        AlwaysAssertExit(sel1.nrow() == 5);
        AlwaysAssertExit(sel2.nrow() == 5);

        // Intersection: rows 4,5,6
        Table tand = sel1 & sel2;
        AlwaysAssertExit(tand.nrow() == 3);

        // Union: rows 2,3,4,5,6,7,8
        Table tor = sel1 | sel2;
        AlwaysAssertExit(tor.nrow() == 7);

        // Difference: rows 2,3
        Table tsub = sel1 - sel2;
        AlwaysAssertExit(tsub.nrow() == 2);

        // Xor: rows 2,3,7,8
        Table txor = sel1 ^ sel2;
        AlwaysAssertExit(txor.nrow() == 4);

        // Not (complement): rows 0,1,7,8,9
        Table tnot = !sel1;
        AlwaysAssertExit(tnot.nrow() == 5);
    }
    deleteIfExists(name);
}

void testMakeIterator()
{
    std::cout << "testMakeIterator" << std::endl;
    String name = uniqueName("tBaseTab_iter");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 6);
        // Give rows grouped values
        ScalarColumn<String> strCol(tab, "StringCol");
        strCol.put(0, "A"); strCol.put(1, "B"); strCol.put(2, "A");
        strCol.put(3, "B"); strCol.put(4, "A"); strCol.put(5, "C");

        TableIterator iter(tab, "StringCol");
        uInt ngroups = 0;
        while (!iter.pastEnd()) {
            ngroups++;
            iter.next();
        }
        AlwaysAssertExit(ngroups == 3);  // A, B, C
    }
    deleteIfExists(name);
}

void testShowStructure()
{
    std::cout << "testShowStructure" << std::endl;
    String name = uniqueName("tBaseTab_show");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);
        std::ostringstream oss;
        tab.showStructure(oss, True, True, False, True, False);
        String output = oss.str();
        AlwaysAssertExit(output.contains("3 rows"));
        AlwaysAssertExit(output.contains("IntCol"));
        AlwaysAssertExit(output.contains("DoubleCol"));
        AlwaysAssertExit(output.contains("StringCol"));
    }
    deleteIfExists(name);
}

void testCheckRemoveColumn()
{
    std::cout << "testCheckRemoveColumn" << std::endl;
    String name = uniqueName("tBaseTab_rmcol");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);

        // Removing a non-existent column should throw
        expectThrows([&]() {
            Vector<String> cols(1);
            cols(0) = "NoSuchColumn";
            tab.removeColumn(cols);
        });

        // Duplicate column name should throw
        expectThrows([&]() {
            Vector<String> cols(2);
            cols(0) = "IntCol";
            cols(1) = "IntCol";
            tab.removeColumn(cols);
        });
    }
    deleteIfExists(name);
}

void testRowRemoval()
{
    std::cout << "testRowRemoval" << std::endl;
    String name = uniqueName("tBaseTab_rmrow");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 10);
        AlwaysAssertExit(tab.nrow() == 10);

        // Remove single row
        tab.removeRow(5);
        AlwaysAssertExit(tab.nrow() == 9);

        // Remove multiple rows via vector
        Vector<rownr_t> rows(2);
        rows(0) = 2; rows(1) = 6;
        tab.removeRow(rows);
        AlwaysAssertExit(tab.nrow() == 7);
    }
    deleteIfExists(name);
}

void testCheckRowNumber()
{
    std::cout << "testCheckRowNumber" << std::endl;
    String name = uniqueName("tBaseTab_chkrow");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);

        // Out-of-range row number should throw
        expectThrows([&]() {
            ScalarColumn<Int> col(tab, "IntCol");
            col.get(100);
        });
    }
    deleteIfExists(name);
}

void testGetPartNames()
{
    std::cout << "testGetPartNames" << std::endl;
    String name = uniqueName("tBaseTab_parts");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);
        Block<String> names = tab.getPartNames(False);
        AlwaysAssertExit(names.size() == 1);
        AlwaysAssertExit(names[0] == tab.tableName());
    }
    deleteIfExists(name);
}

void testColumnInfo()
{
    std::cout << "testColumnInfo" << std::endl;
    String name = uniqueName("tBaseTab_colinfo");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);

        // isColumnWritable
        AlwaysAssertExit(tab.isColumnWritable("IntCol"));
        AlwaysAssertExit(tab.isColumnWritable(0u));

        // isColumnStored
        AlwaysAssertExit(tab.isColumnStored("IntCol"));
        AlwaysAssertExit(tab.isColumnStored(0u));

        // read-only table
        tab.flush();
    }
    {
        Table tab(name, Table::Old);
        AlwaysAssertExit(!tab.isColumnWritable("IntCol"));
        AlwaysAssertExit(!tab.isColumnWritable(0u));
    }
    deleteIfExists(name);
}

void testTableInfo()
{
    std::cout << "testTableInfo" << std::endl;
    String name = uniqueName("tBaseTab_info");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 1);
        tab.tableInfo().setType("TestType");
        tab.tableInfo().setSubType("TestSub");
        tab.flush();
    }
    {
        Table tab(name);
        AlwaysAssertExit(tab.tableInfo().type() == "TestType");
        AlwaysAssertExit(tab.tableInfo().subType() == "TestSub");
    }
    // Static tableInfo (read from disk)
    {
        TableInfo info(Path(name).absoluteName() + "/table.info");
        AlwaysAssertExit(info.type() == "TestType");
    }
    deleteIfExists(name);
}

void testMakeAbsoluteNameErrors()
{
    std::cout << "testMakeAbsoluteNameErrors" << std::endl;
    // Table name with only dots and slashes should throw
    expectThrows([&]() {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("col"));
        SetupNewTable newtab("../.", td, Table::New);
        Table tab(newtab, 0);
    });
}

void testAddColumnsViaDmInfo()
{
    std::cout << "testAddColumnsViaDmInfo" << std::endl;
    String name = uniqueName("tBaseTab_addcol");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);

        // Add a column using dminfo record
        TableDesc addTd;
        addTd.addColumn(ScalarColumnDesc<Float>("NewFloat"));
        Record dmInfo;
        dmInfo.define("TYPE", "StandardStMan");
        dmInfo.define("NAME", "SSM_new");
        tab.addColumn(addTd, dmInfo);

        AlwaysAssertExit(tab.tableDesc().isColumn("NewFloat"));
        ScalarColumn<Float> newCol(tab, "NewFloat");
        // Should be accessible (default values)
        Float val = newCol(0);
        (void)val;
    }

    // Invalid dmInfo should throw
    {
        Table tab(name, Table::Update);
        TableDesc addTd;
        addTd.addColumn(ScalarColumnDesc<Int>("BadCol"));
        Record badInfo;
        badInfo.define("WRONG_FIELD", 42);
        expectThrows([&]() { tab.addColumn(addTd, badInfo); });
    }
    deleteIfExists(name);
}

void testRowNumbers()
{
    std::cout << "testRowNumbers" << std::endl;
    String name = uniqueName("tBaseTab_rownrs");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);
        Vector<rownr_t> rownrs = tab.rowNumbers();
        AlwaysAssertExit(rownrs.size() == 5);
        for (uInt i = 0; i < 5; i++) {
            AlwaysAssertExit(rownrs(i) == rownr_t(i));
        }

        // RefTable row numbers (after select)
        Table sel = tab(tab.col("IntCol") >= 20);
        Vector<rownr_t> selRows = sel.rowNumbers();
        AlwaysAssertExit(selRows.size() == 3);

        // RefTable row numbers relative to root
        Vector<rownr_t> rootRows = sel.rowNumbers(tab);
        AlwaysAssertExit(rootRows.size() == 3);
    }
    deleteIfExists(name);
}

}  // namespace

int main()
{
    try {
        testConstruction();
        testOpenedForWrite();
        testMarkForDelete();
        testRename();
        testCopyAndDeepCopy();
        testSelectRows();
        testProject();
        testSort();
        testSetOperations();
        testMakeIterator();
        testShowStructure();
        testCheckRemoveColumn();
        testRowRemoval();
        testCheckRowNumber();
        testGetPartNames();
        testColumnInfo();
        testTableInfo();
        testMakeAbsoluteNameErrors();
        testAddColumnsViaDmInfo();
        testRowNumbers();
    } catch (const std::exception& e) {
        std::cerr << "FAIL: " << e.what() << std::endl;
        return 1;
    }
    std::cout << "OK" << std::endl;
    return 0;
}
