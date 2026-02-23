//# tColumnSetCoverage.cc: characterization coverage for ColumnSet code paths
//#
//# Exercises (all through the public Table/SetupNewTable/Column API):
//#   addColumn overloads (by ColumnDesc only, by DM name, by DM type,
//#     with explicit DataManager object),
//#   removeColumn (partial removal from multi-column DM, entire-DM deletion),
//#   renameColumn,
//#   uniqueDataManagerName (_N suffix generation),
//#   canAddRow / canRemoveRow / canRemoveColumn / canRenameColumn predicates,
//#   dataManagerInfo / actualTableDesc reflection,
//#   addRow / removeRow propagation,
//#   resync via flush-and-reopen,
//#   checkDataManagerNames (no-duplicate invariant),
//#   areTablesMultiUsed,
//#   getColumn by name and by index,
//#   reopenRW.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableColumn.h>
#include <casacore/tables/Tables/TableRecord.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/tables/DataMan/StandardStMan.h>
#include <casacore/tables/DataMan/TiledCellStMan.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/OS/Path.h>

#include <functional>
#include <iostream>
#include <unistd.h>

using namespace casacore;
using namespace std;

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

// Helper: find a sub-record in dataManagerInfo() whose NAME field equals dmName.
// Returns the index, or -1 if not found.
Int findDMInfoByName(const Record& dmInfo, const String& dmName)
{
    for (uInt i = 0; i < dmInfo.nfields(); i++) {
        const Record& sub = dmInfo.subRecord(i);
        if (sub.asString("NAME") == dmName) {
            return Int(i);
        }
    }
    return -1;
}

// Helper: collect all DM NAMEs from a dataManagerInfo record.
Vector<String> allDMNames(const Record& dmInfo)
{
    Vector<String> names(dmInfo.nfields());
    for (uInt i = 0; i < dmInfo.nfields(); i++) {
        names(i) = dmInfo.subRecord(i).asString("NAME");
    }
    return names;
}

// -----------------------------------------------------------------------
//  1. addColumn overloads
// -----------------------------------------------------------------------

void testAddColumnOverloads()
{
    cout << "testAddColumnOverloads" << endl;
    String name = uniqueName("tColSet_addcol");
    deleteIfExists(name);
    {
        // Create an initial table with two storage managers.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("OrigInt"));
        td.addColumn(ScalarColumnDesc<Double>("OrigDbl"));

        StandardStMan ssm("MySSM");
        IncrementalStMan ism("MyISM");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("OrigInt", ssm);
        newtab.bindColumn("OrigDbl", ism);
        Table tab(newtab, 5);

        // Populate original columns.
        ScalarColumn<Int> origInt(tab, "OrigInt");
        ScalarColumn<Double> origDbl(tab, "OrigDbl");
        for (uInt i = 0; i < 5; i++) {
            origInt.put(i, Int(i * 10));
            origDbl.put(i, Double(i) * 1.5);
        }

        // (a) addColumn with ColumnDesc only — finds an existing DM that
        //     can accept the column.
        tab.addColumn(ScalarColumnDesc<Float>("AddedFloat"));
        AlwaysAssertExit(tab.tableDesc().isColumn("AddedFloat"));

        // (b) addColumn by DM name — add to "MySSM" by name.
        //     (IncrementalStMan does not support addColumn, so use SSM.)
        tab.addColumn(ScalarColumnDesc<String>("AddedStr"),
                      "MySSM", True);  // byName=True
        AlwaysAssertExit(tab.tableDesc().isColumn("AddedStr"));
        // Verify it landed in the SSM.
        {
            Record dmi = tab.dataManagerInfo();
            Int idx = findDMInfoByName(dmi, "MySSM");
            AlwaysAssertExit(idx >= 0);
            Vector<String> cols =
                dmi.subRecord(idx).asArrayString("COLUMNS");
            Bool found = False;
            for (uInt i = 0; i < cols.nelements(); i++) {
                if (cols(i) == "AddedStr") found = True;
            }
            AlwaysAssertExit(found);
        }

        // (c) addColumn by DM type — finds existing DM of that type.
        tab.addColumn(ScalarColumnDesc<Int>("AddedInt2"),
                      "StandardStMan", False);  // byName=False
        AlwaysAssertExit(tab.tableDesc().isColumn("AddedInt2"));

        // (d) addColumn with explicit DataManager object — creates a new DM.
        StandardStMan newSsm("ExplicitSSM");
        tab.addColumn(ScalarColumnDesc<Double>("ExplicitDbl"), newSsm);
        AlwaysAssertExit(tab.tableDesc().isColumn("ExplicitDbl"));
        {
            Record dmi = tab.dataManagerInfo();
            Int idx = findDMInfoByName(dmi, "ExplicitSSM");
            AlwaysAssertExit(idx >= 0);
        }

        // Also test adding an array column.
        IncrementalStMan arrayIsm("ArrayISM");
        tab.addColumn(ArrayColumnDesc<Float>("AddedArr"), arrayIsm);
        AlwaysAssertExit(tab.tableDesc().isColumn("AddedArr"));

        // Verify we can write and read back through the new columns.
        ScalarColumn<Float> addedFloat(tab, "AddedFloat");
        addedFloat.put(0, 3.14f);
        AlwaysAssertExit(addedFloat(0) == 3.14f);

        ScalarColumn<String> addedStr(tab, "AddedStr");
        addedStr.put(0, "hello");
        AlwaysAssertExit(addedStr(0) == "hello");

        ScalarColumn<Double> explDbl(tab, "ExplicitDbl");
        explDbl.put(2, 2.718);
        AlwaysAssertExit(explDbl(2) == 2.718);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  2. removeColumn — partial and entire-DM deletion
// -----------------------------------------------------------------------

void testRemoveColumnPartial()
{
    cout << "testRemoveColumnPartial" << endl;
    String name = uniqueName("tColSet_rmpart");
    deleteIfExists(name);
    {
        // Create a table where one DM owns two columns.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("ColA"));
        td.addColumn(ScalarColumnDesc<Int>("ColB"));
        td.addColumn(ScalarColumnDesc<Double>("ColC"));

        StandardStMan ssm("SharedSSM");
        IncrementalStMan ism("SoloISM");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("ColA", ssm);
        newtab.bindColumn("ColB", ssm);
        newtab.bindColumn("ColC", ism);
        Table tab(newtab, 3);

        ScalarColumn<Int> colA(tab, "ColA");
        ScalarColumn<Int> colB(tab, "ColB");
        for (uInt i = 0; i < 3; i++) {
            colA.put(i, Int(i));
            colB.put(i, Int(i * 100));
        }

        // Remove ColA — partial removal from SharedSSM (DM still has ColB).
        tab.removeColumn("ColA");
        AlwaysAssertExit(!tab.tableDesc().isColumn("ColA"));
        AlwaysAssertExit(tab.tableDesc().isColumn("ColB"));

        // Verify SharedSSM still exists and serves ColB.
        {
            Record dmi = tab.dataManagerInfo();
            Int idx = findDMInfoByName(dmi, "SharedSSM");
            AlwaysAssertExit(idx >= 0);
            Vector<String> cols =
                dmi.subRecord(idx).asArrayString("COLUMNS");
            AlwaysAssertExit(cols.nelements() == 1);
            AlwaysAssertExit(cols(0) == "ColB");
        }

        // Verify ColB data is intact.
        ScalarColumn<Int> colBcheck(tab, "ColB");
        AlwaysAssertExit(colBcheck(0) == 0);
        AlwaysAssertExit(colBcheck(1) == 100);
        AlwaysAssertExit(colBcheck(2) == 200);
    }
    deleteIfExists(name);
}

void testRemoveColumnEntireDM()
{
    cout << "testRemoveColumnEntireDM" << endl;
    String name = uniqueName("tColSet_rmentire");
    deleteIfExists(name);
    {
        // Create a table with a DM that owns exactly one column.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Keep1"));
        td.addColumn(ScalarColumnDesc<Double>("Keep2"));
        td.addColumn(ScalarColumnDesc<String>("Victim"));

        StandardStMan ssm1("KeepSSM");
        IncrementalStMan ism("VictimISM");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("Keep1", ssm1);
        newtab.bindColumn("Keep2", ssm1);
        newtab.bindColumn("Victim", ism);
        Table tab(newtab, 4);

        // Populate
        ScalarColumn<Int> keep1(tab, "Keep1");
        ScalarColumn<String> victim(tab, "Victim");
        for (uInt i = 0; i < 4; i++) {
            keep1.put(i, Int(i));
            victim.put(i, "v" + String::toString(i));
        }

        // Confirm VictimISM exists before removal.
        {
            Record dmi = tab.dataManagerInfo();
            AlwaysAssertExit(findDMInfoByName(dmi, "VictimISM") >= 0);
        }

        // Remove the only column in VictimISM — triggers entire-DM deletion
        // (the count=-1 path with objmove+resize of blockDataMan_p).
        tab.removeColumn("Victim");
        AlwaysAssertExit(!tab.tableDesc().isColumn("Victim"));

        // VictimISM should no longer appear in dataManagerInfo.
        {
            Record dmi = tab.dataManagerInfo();
            AlwaysAssertExit(findDMInfoByName(dmi, "VictimISM") < 0);
        }

        // Remaining data should be intact.
        ScalarColumn<Int> keep1check(tab, "Keep1");
        for (uInt i = 0; i < 4; i++) {
            AlwaysAssertExit(keep1check(i) == Int(i));
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  3. renameColumn
// -----------------------------------------------------------------------

void testRenameColumn()
{
    cout << "testRenameColumn" << endl;
    String name = uniqueName("tColSet_rename");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("OldName"));
        td.addColumn(ScalarColumnDesc<Double>("Other"));
        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 5);

        ScalarColumn<Int> col(tab, "OldName");
        for (uInt i = 0; i < 5; i++) col.put(i, Int(i * 7));

        // Rename
        tab.renameColumn("NewName", "OldName");
        AlwaysAssertExit(tab.tableDesc().isColumn("NewName"));
        AlwaysAssertExit(!tab.tableDesc().isColumn("OldName"));

        // Data accessible under new name.
        ScalarColumn<Int> renamed(tab, "NewName");
        for (uInt i = 0; i < 5; i++) {
            AlwaysAssertExit(renamed(i) == Int(i * 7));
        }

        // Renaming to an existing name should throw.
        expectThrows([&]() {
            tab.renameColumn("Other", "NewName");
        });

        // Renaming a non-existent column should throw.
        expectThrows([&]() {
            tab.renameColumn("Foo", "NoSuchCol");
        });
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  4. uniqueDataManagerName — _N suffix generation
// -----------------------------------------------------------------------

void testUniqueDataManagerName()
{
    cout << "testUniqueDataManagerName" << endl;
    String name = uniqueName("tColSet_uniqDM");
    deleteIfExists(name);
    {
        // Create a table with a DM named "StandardStMan".
        // The default DM group name for a ScalarColumnDesc is "StandardStMan".
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));

        StandardStMan ssm("StandardStMan");
        SetupNewTable newtab(name, td, Table::New);
        newtab.bindAll(ssm);
        Table tab(newtab, 3);

        // Now add a column using the single-argument addColumn(ColumnDesc).
        // This path internally calls uniqueDataManagerName when it cannot find
        // an existing DM that can accept the column (none exists in this case
        // because SSM can accept it). But if we use addColumn by DM type with
        // a type name that matches a DM that cannot accept more columns, a new
        // DM is created with uniqueDataManagerName.
        //
        // Simpler approach: use the addColumn(ColumnDesc, String, byName=False)
        // overload with a type that doesn't match any existing DM type, so a
        // new DM is created. The created DM name will be unique-ified.
        //
        // Actually, the simplest approach: the single-argument addColumn finds
        // the existing SSM (which can accept columns) and adds to it. So the
        // uniqueDataManagerName path is NOT exercised there.
        //
        // To exercise uniqueDataManagerName, we use the by-type path:
        //   addColumn(ColumnDesc, "IncrementalStMan", byName=False)
        // Since no ISM exists, it creates one named "IncrementalStMan".
        tab.addColumn(ScalarColumnDesc<Double>("Col2"),
                      "IncrementalStMan", False);

        // Add another ISM column by type. The existing ISM does not support
        // addColumn, so a new ISM is created. Since "IncrementalStMan" is
        // already taken, uniqueDataManagerName appends "_1".
        tab.addColumn(ScalarColumnDesc<Float>("Col3"),
                      "IncrementalStMan", False);

        Record dmi = tab.dataManagerInfo();
        Vector<String> names = allDMNames(dmi);

        // We should have "StandardStMan", "IncrementalStMan",
        // and "IncrementalStMan_1".
        Bool foundISM = False;
        Bool foundISM1 = False;
        for (uInt i = 0; i < names.nelements(); i++) {
            if (names(i) == "IncrementalStMan") foundISM = True;
            if (names(i) == "IncrementalStMan_1") foundISM1 = True;
        }
        AlwaysAssertExit(foundISM);
        AlwaysAssertExit(foundISM1);

        // Add yet another — should get "IncrementalStMan_2".
        tab.addColumn(ScalarColumnDesc<String>("Col4"),
                      "IncrementalStMan", False);

        dmi = tab.dataManagerInfo();
        names = allDMNames(dmi);
        Bool foundISM2 = False;
        for (uInt i = 0; i < names.nelements(); i++) {
            if (names(i) == "IncrementalStMan_2") foundISM2 = True;
        }
        AlwaysAssertExit(foundISM2);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  5. canAddRow / canRemoveRow / canRemoveColumn / canRenameColumn
// -----------------------------------------------------------------------

void testCanPredicates()
{
    cout << "testCanPredicates" << endl;
    String name = uniqueName("tColSet_canpred");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));
        td.addColumn(ScalarColumnDesc<Double>("Col2"));

        StandardStMan ssm("SSM");
        SetupNewTable newtab(name, td, Table::New);
        newtab.bindAll(ssm);
        Table tab(newtab, 3);

        // StandardStMan supports add/remove rows and columns.
        AlwaysAssertExit(tab.canAddRow());
        AlwaysAssertExit(tab.canRemoveRow());
        AlwaysAssertExit(tab.canRemoveColumn("Col1"));
        AlwaysAssertExit(tab.canRenameColumn("Col1"));

        // Non-existent column: canRemoveColumn should return False.
        AlwaysAssertExit(!tab.canRemoveColumn("NoSuchCol"));

        // canRenameColumn on non-existent column: should return False.
        AlwaysAssertExit(!tab.canRenameColumn("NoSuchCol"));

        // Vector form of canRemoveColumn.
        Vector<String> cols(2);
        cols(0) = "Col1";
        cols(1) = "Col2";
        AlwaysAssertExit(tab.canRemoveColumn(cols));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  6. dataManagerInfo / actualTableDesc
// -----------------------------------------------------------------------

void testDataManagerInfoAndActualDesc()
{
    cout << "testDataManagerInfoAndActualDesc" << endl;
    String name = uniqueName("tColSet_dmiinfo");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("IntCol"));
        td.addColumn(ScalarColumnDesc<Double>("DblCol"));
        td.addColumn(ArrayColumnDesc<Float>("ArrCol"));

        StandardStMan ssm("SSM1");
        IncrementalStMan ism("ISM1");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("IntCol", ssm);
        newtab.bindColumn("DblCol", ism);
        newtab.bindColumn("ArrCol", ssm);
        Table tab(newtab, 4);

        // dataManagerInfo should return two DMs.
        Record dmi = tab.dataManagerInfo();
        AlwaysAssertExit(dmi.nfields() == 2);

        // Both SSM1 and ISM1 should be present.
        AlwaysAssertExit(findDMInfoByName(dmi, "SSM1") >= 0);
        AlwaysAssertExit(findDMInfoByName(dmi, "ISM1") >= 0);

        // SSM1 should have IntCol and ArrCol.
        {
            Int idx = findDMInfoByName(dmi, "SSM1");
            Vector<String> cols =
                dmi.subRecord(idx).asArrayString("COLUMNS");
            AlwaysAssertExit(cols.nelements() == 2);
        }

        // ISM1 should have DblCol.
        {
            Int idx = findDMInfoByName(dmi, "ISM1");
            Vector<String> cols =
                dmi.subRecord(idx).asArrayString("COLUMNS");
            AlwaysAssertExit(cols.nelements() == 1);
            AlwaysAssertExit(cols(0) == "DblCol");
        }

        // actualTableDesc should reflect the actual DM types.
        TableDesc atd = tab.actualTableDesc();
        AlwaysAssertExit(atd.ncolumn() == 3);
        AlwaysAssertExit(atd.isColumn("IntCol"));
        AlwaysAssertExit(atd.isColumn("DblCol"));
        AlwaysAssertExit(atd.isColumn("ArrCol"));

        // The DM type for IntCol should be StandardStMan.
        AlwaysAssertExit(
            atd.columnDesc("IntCol").dataManagerType() == "StandardStMan");
        // The DM type for DblCol should be IncrementalStMan.
        AlwaysAssertExit(
            atd.columnDesc("DblCol").dataManagerType() == "IncrementalStMan");

        // After mutation, info should update.
        tab.removeColumn("ArrCol");
        Record dmi2 = tab.dataManagerInfo();
        {
            Int idx = findDMInfoByName(dmi2, "SSM1");
            AlwaysAssertExit(idx >= 0);
            Vector<String> cols =
                dmi2.subRecord(idx).asArrayString("COLUMNS");
            AlwaysAssertExit(cols.nelements() == 1);
            AlwaysAssertExit(cols(0) == "IntCol");
        }

        TableDesc atd2 = tab.actualTableDesc();
        AlwaysAssertExit(atd2.ncolumn() == 2);
        AlwaysAssertExit(!atd2.isColumn("ArrCol"));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  7. addRow / removeRow propagation
// -----------------------------------------------------------------------

void testAddRemoveRow()
{
    cout << "testAddRemoveRow" << endl;
    String name = uniqueName("tColSet_addrmrow");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));
        td.addColumn(ScalarColumnDesc<Double>("Col2"));

        StandardStMan ssm("SSM");
        IncrementalStMan ism("ISM");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("Col1", ssm);
        newtab.bindColumn("Col2", ism);
        Table tab(newtab, 0);
        AlwaysAssertExit(tab.nrow() == 0);

        // Add rows.
        tab.addRow(5);
        AlwaysAssertExit(tab.nrow() == 5);

        // Populate.
        ScalarColumn<Int> col1(tab, "Col1");
        ScalarColumn<Double> col2(tab, "Col2");
        for (uInt i = 0; i < 5; i++) {
            col1.put(i, Int(i));
            col2.put(i, Double(i) * 0.5);
        }

        // Add more rows.
        tab.addRow(3);
        AlwaysAssertExit(tab.nrow() == 8);
        col1.put(5, 50);
        col1.put(6, 60);
        col1.put(7, 70);

        // Remove a row.
        tab.removeRow(2);
        AlwaysAssertExit(tab.nrow() == 7);

        // Verify data integrity after removal.
        // Row 2 was removed; original rows 0,1,3,4,5,6,7 remain.
        AlwaysAssertExit(col1(0) == 0);
        AlwaysAssertExit(col1(1) == 1);
        // After removing row 2, what was row 3 is now row 2.
        AlwaysAssertExit(col1(2) == 3);

        // Remove out-of-range row should throw.
        expectThrows([&]() { tab.removeRow(100); });
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  8. resync via flush-and-reopen
// -----------------------------------------------------------------------

void testResyncViaReopen()
{
    cout << "testResyncViaReopen" << endl;
    String name = uniqueName("tColSet_resync");
    deleteIfExists(name);
    {
        // Create and populate.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("IntCol"));
        td.addColumn(ScalarColumnDesc<Double>("DblCol"));

        StandardStMan ssm("SSM");
        SetupNewTable newtab(name, td, Table::New);
        newtab.bindAll(ssm);
        Table tab(newtab, 5);

        ScalarColumn<Int> intCol(tab, "IntCol");
        ScalarColumn<Double> dblCol(tab, "DblCol");
        for (uInt i = 0; i < 5; i++) {
            intCol.put(i, Int(i * 11));
            dblCol.put(i, Double(i) * 2.2);
        }

        // Flush to disk.
        tab.flush();
    }
    {
        // Reopen and verify data consistency (exercises resync path).
        Table tab(name);
        AlwaysAssertExit(tab.nrow() == 5);

        ScalarColumn<Int> intCol(tab, "IntCol");
        ScalarColumn<Double> dblCol(tab, "DblCol");
        for (uInt i = 0; i < 5; i++) {
            AlwaysAssertExit(intCol(i) == Int(i * 11));
            AlwaysAssertExit(dblCol(i) == Double(i) * 2.2);
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  9. checkDataManagerNames — no duplicates invariant
// -----------------------------------------------------------------------

void testCheckDataManagerNames()
{
    cout << "testCheckDataManagerNames" << endl;
    String name = uniqueName("tColSet_chknames");
    deleteIfExists(name);
    {
        // Create a table with distinctly named DMs.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));
        td.addColumn(ScalarColumnDesc<Double>("Col2"));

        StandardStMan ssm("SSM_A");
        IncrementalStMan ism("ISM_B");

        SetupNewTable newtab(name, td, Table::New);
        newtab.bindColumn("Col1", ssm);
        newtab.bindColumn("Col2", ism);
        Table tab(newtab, 2);

        // Verify both DMs are present and named uniquely.
        Record dmi = tab.dataManagerInfo();
        Vector<String> names = allDMNames(dmi);
        AlwaysAssertExit(names.nelements() == 2);
        // Make sure they are different.
        AlwaysAssertExit(names(0) != names(1));

        // Attempting to add a DM with a duplicate name (but going through
        // the addColumn(TableDesc, DataManager) path, which calls
        // checkDataManagerName) should throw.
        // The table already has "SSM_A"; creating another with the same
        // name via addColumn(TableDesc, DataManager) hits checkDataManagerName
        // which throws.
        expectThrows([&]() {
            TableDesc addTd;
            addTd.addColumn(ScalarColumnDesc<Float>("Col3"));
            StandardStMan ssmDup("SSM_A");
            tab.addColumn(addTd, ssmDup);
        });
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  10. areTablesMultiUsed
// -----------------------------------------------------------------------

void testAreTablesMultiUsed()
{
    cout << "testAreTablesMultiUsed" << endl;
    String name = uniqueName("tColSet_multiused");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));
        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 2);

        // For a simple table without subtable references in column keywords,
        // areTablesMultiUsed should be False.
        AlwaysAssertExit(!tab.isMultiUsed());
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  11. getColumn by name and by index (via TableColumn)
// -----------------------------------------------------------------------

void testGetColumnByNameAndIndex()
{
    cout << "testGetColumnByNameAndIndex" << endl;
    String name = uniqueName("tColSet_getcol");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Alpha"));
        td.addColumn(ScalarColumnDesc<Double>("Beta"));
        td.addColumn(ScalarColumnDesc<String>("Gamma"));

        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 3);

        ScalarColumn<Int> alpha(tab, "Alpha");
        ScalarColumn<Double> beta(tab, "Beta");
        ScalarColumn<String> gamma(tab, "Gamma");
        for (uInt i = 0; i < 3; i++) {
            alpha.put(i, Int(i));
            beta.put(i, Double(i) * 0.1);
            gamma.put(i, "g" + String::toString(i));
        }

        // Access by name.
        {
            TableColumn tc(tab, "Alpha");
            AlwaysAssertExit(tc.columnDesc().name() == "Alpha");
        }
        // Access by index (columns are numbered in order of addition).
        {
            TableColumn tc(tab, 0);
            AlwaysAssertExit(tc.columnDesc().name() == "Alpha");
        }
        {
            TableColumn tc(tab, 1);
            AlwaysAssertExit(tc.columnDesc().name() == "Beta");
        }
        {
            TableColumn tc(tab, 2);
            AlwaysAssertExit(tc.columnDesc().name() == "Gamma");
        }

        // Read data through indexed access (use TableColumn, then wrap).
        {
            TableColumn tc0(tab, 0);
            ScalarColumn<Int> byIdx(tc0);
            AlwaysAssertExit(byIdx(0) == 0);
            AlwaysAssertExit(byIdx(1) == 1);
            AlwaysAssertExit(byIdx(2) == 2);
        }
        {
            TableColumn tc1(tab, 1);
            ScalarColumn<Double> byIdx(tc1);
            AlwaysAssertExit(byIdx(0) == 0.0);
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  12. reopenRW
// -----------------------------------------------------------------------

void testReopenRW()
{
    cout << "testReopenRW" << endl;
    String name = uniqueName("tColSet_reopenRW");
    deleteIfExists(name);
    {
        // Create and flush.
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("Col1"));
        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 3);
        ScalarColumn<Int> col(tab, "Col1");
        for (uInt i = 0; i < 3; i++) col.put(i, Int(i));
        tab.flush();
    }
    {
        // Open read-only.
        Table tab(name, Table::Old);
        AlwaysAssertExit(!tab.isWritable());

        // Reading should work.
        ScalarColumn<Int> col(tab, "Col1");
        AlwaysAssertExit(col(0) == 0);
        AlwaysAssertExit(col(1) == 1);
        AlwaysAssertExit(col(2) == 2);

        // Reopen for writing.
        tab.reopenRW();
        AlwaysAssertExit(tab.isWritable());

        // Writing should succeed.
        ScalarColumn<Int> colRW(tab, "Col1");
        colRW.put(0, 999);
        AlwaysAssertExit(colRW(0) == 999);
        tab.flush();
    }
    {
        // Reopen to verify the write persisted.
        Table tab(name);
        ScalarColumn<Int> col(tab, "Col1");
        AlwaysAssertExit(col(0) == 999);
    }
    deleteIfExists(name);
}

}  // anonymous namespace

int main()
{
    try {
        testAddColumnOverloads();
        testRemoveColumnPartial();
        testRemoveColumnEntireDM();
        testRenameColumn();
        testUniqueDataManagerName();
        testCanPredicates();
        testDataManagerInfoAndActualDesc();
        testAddRemoveRow();
        testResyncViaReopen();
        testCheckDataManagerNames();
        testAreTablesMultiUsed();
        testGetColumnByNameAndIndex();
        testReopenRW();
    } catch (const std::exception& e) {
        cerr << "FAIL: " << e.what() << endl;
        return 1;
    }
    cout << "All ColumnSet coverage tests passed." << endl;
    return 0;
}
