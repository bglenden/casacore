//# tPlainTableCoverage.cc: characterization coverage for PlainTable code paths
//#
//# Exercises (through the public Table API):
//#   changeTiledDataOnly + flush, reopenRW, toAipsIOFoption (indirectly via
//#   different TableOption values), setEndian / asBigEndian / endianFormat,
//#   getLayout, isMultiUsed, hasDataChanged (exercises getModifyCounter),
//#   keywordSet vs rwKeywordSet, renameHypercolumn, addRow with initialize,
//#   isWritable, storageOption, lock/unlock/hasLock, flush with recursive,
//#   actualTableDesc, dataManagerInfo, findDataManager, canAddRow/canRemoveRow,
//#   canRemoveColumn, canRenameColumn.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableRecord.h>
#include <casacore/tables/Tables/TableLock.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/tables/DataMan/StandardStMan.h>
#include <casacore/tables/DataMan/TiledCellStMan.h>
#include <casacore/tables/DataMan/TiledColumnStMan.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/OS/Path.h>
#include <casacore/casa/OS/Directory.h>
#include <casacore/casa/OS/HostInfo.h>
#include <casacore/casa/Exceptions/Error.h>
#include <casacore/casa/IO/FileLocker.h>

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

// Create a simple table with scalar Int, Double, and String columns.
// The TableLock and endianFormat are passed to the Table constructor
// using the (SetupNewTable&, const TableLock&, rownr_t, Bool, EndianFormat)
// overload.
Table makeSimpleTable(const String& name, uInt nrow,
                      Table::TableOption opt = Table::New,
                      const TableLock& lock = TableLock(TableLock::AutoLocking),
                      int endianFormat = Table::LocalEndian)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("IntCol"));
    td.addColumn(ScalarColumnDesc<Double>("DoubleCol"));
    td.addColumn(ScalarColumnDesc<String>("StringCol"));
    SetupNewTable newtab(name, td, opt);
    Table tab(newtab, lock, nrow, False,
              static_cast<Table::EndianFormat>(endianFormat));
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

// Create a table with a TiledColumnStMan for array data.
Table makeTiledTable(const String& name, uInt nrow)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("IntCol"));
    td.addColumn(ArrayColumnDesc<Float>("TiledArr",
                                         IPosition(1, 8),
                                         ColumnDesc::FixedShape));
    // Define a hypercolumn for the tiled data
    td.defineHypercolumn("TiledHC", 2,
                         Vector<String>(1, "TiledArr"));

    SetupNewTable newtab(name, td, Table::New);
    // Bind the tiled column to a TiledColumnStMan
    TiledColumnStMan tsm("TiledSM", IPosition(2, 8, 4));
    newtab.bindColumn("TiledArr", tsm);

    Table tab(newtab, nrow);
    ScalarColumn<Int> intCol(tab, "IntCol");
    ArrayColumn<Float> arrCol(tab, "TiledArr");
    for (uInt i = 0; i < nrow; i++) {
        intCol.put(i, Int(i));
        Vector<Float> v(8);
        v = Float(i);
        arrCol.put(i, v);
    }
    return tab;
}

// -----------------------------------------------------------------------
//  Test: changeTiledDataOnly + flush
// -----------------------------------------------------------------------
void testChangeTiledDataOnly()
{
    cout << "testChangeTiledDataOnly" << endl;
    String name = uniqueName("tPTCov_tiled");
    deleteIfExists(name);
    {
        // Create a tiled table and close it
        {
            Table tab = makeTiledTable(name, 8);
            tab.flush();
        }

        // Reopen for update, set changeTiledDataOnly, modify tiled data, flush
        {
            Table tab(name, Table::Update);
            tab.changeTiledDataOnly();

            // Modify the tiled array data
            ArrayColumn<Float> arrCol(tab, "TiledArr");
            Vector<Float> v(8);
            v = 99.0f;
            arrCol.put(0, v);

            // Flush -- should only write DM data, not table header
            tab.flush();

            // Verify the data was written
            Vector<Float> readback = arrCol(0);
            AlwaysAssertExit(allEQ(readback, Float(99.0f)));
        }

        // Reopen and verify persistence
        {
            Table tab(name, Table::Old);
            ArrayColumn<Float> arrCol(tab, "TiledArr");
            Vector<Float> readback = arrCol(0);
            AlwaysAssertExit(allEQ(readback, Float(99.0f)));
            // Other rows should be unchanged
            Vector<Float> row1 = arrCol(1);
            AlwaysAssertExit(allEQ(row1, Float(1.0f)));
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: reopenRW
// -----------------------------------------------------------------------
void testReopenRW()
{
    cout << "testReopenRW" << endl;
    String name = uniqueName("tPTCov_reopenrw");
    deleteIfExists(name);
    {
        // Create and close
        {
            Table tab = makeSimpleTable(name, 5);
            tab.flush();
        }

        // Open as read-only, then reopen for read-write
        {
            Table tab(name, Table::Old);
            AlwaysAssertExit(!tab.isWritable());

            tab.reopenRW();
            AlwaysAssertExit(tab.isWritable());

            // Verify that writes succeed after reopenRW
            ScalarColumn<Int> intCol(tab, "IntCol");
            intCol.put(0, 999);
            AlwaysAssertExit(intCol(0) == 999);

            // Test the "already writable" early-return path
            tab.reopenRW();  // should be a no-op
            AlwaysAssertExit(tab.isWritable());

            tab.flush();
        }

        // Verify persistence
        {
            Table tab(name, Table::Old);
            ScalarColumn<Int> intCol(tab, "IntCol");
            AlwaysAssertExit(intCol(0) == 999);
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: toAipsIOFoption (indirectly through different open modes)
// -----------------------------------------------------------------------
void testTableOptions()
{
    cout << "testTableOptions" << endl;
    String nameNew = uniqueName("tPTCov_optNew");
    String nameNNR = uniqueName("tPTCov_optNNR");
    String nameScratch = uniqueName("tPTCov_optScr");
    deleteIfExists(nameNew);
    deleteIfExists(nameNNR);
    deleteIfExists(nameScratch);

    // Table::New
    {
        Table tab = makeSimpleTable(nameNew, 3, Table::New);
        AlwaysAssertExit(tab.nrow() == 3);
        AlwaysAssertExit(tab.isWritable());
        tab.flush();
    }

    // Table::Old (read-only)
    {
        Table tab(nameNew, Table::Old);
        AlwaysAssertExit(!tab.isWritable());
    }

    // Table::Update
    {
        Table tab(nameNew, Table::Update);
        AlwaysAssertExit(tab.isWritable());
    }

    // Table::NewNoReplace
    {
        Table tab = makeSimpleTable(nameNNR, 2, Table::NewNoReplace);
        AlwaysAssertExit(tab.nrow() == 2);
        tab.flush();
    }

    // Table::Scratch
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("col1"));
        SetupNewTable newtab(nameScratch, td, Table::Scratch);
        Table tab(newtab, 1);
        AlwaysAssertExit(tab.isWritable());
        AlwaysAssertExit(tab.isMarkedForDelete());
    }
    // Scratch table should be deleted when closed
    AlwaysAssertExit(!Table::isReadable(nameScratch));

    // Table::Delete
    {
        String nameDel = uniqueName("tPTCov_optDel");
        deleteIfExists(nameDel);
        {
            Table tab = makeSimpleTable(nameDel, 2);
            tab.flush();
        }
        {
            Table tab(nameDel, Table::Delete);
            AlwaysAssertExit(!tab.isWritable());
        }
        // Table should be deleted
        AlwaysAssertExit(!Table::isReadable(nameDel));
    }

    deleteIfExists(nameNew);
    deleteIfExists(nameNNR);
}

// -----------------------------------------------------------------------
//  Test: setEndian / asBigEndian / endianFormat
// -----------------------------------------------------------------------
void testEndianFormat()
{
    cout << "testEndianFormat" << endl;
    String nameBig = uniqueName("tPTCov_bigend");
    String nameLittle = uniqueName("tPTCov_littleend");
    String nameLocal = uniqueName("tPTCov_localend");
    deleteIfExists(nameBig);
    deleteIfExists(nameLittle);
    deleteIfExists(nameLocal);

    // BigEndian
    {
        Table tab = makeSimpleTable(nameBig, 2, Table::New,
                                    TableLock(TableLock::AutoLocking),
                                    Table::BigEndian);
        AlwaysAssertExit(tab.endianFormat() == Table::BigEndian);
        tab.flush();
    }
    // Verify persistence of endian format
    {
        Table tab(nameBig, Table::Old);
        AlwaysAssertExit(tab.endianFormat() == Table::BigEndian);
    }

    // LittleEndian
    {
        Table tab = makeSimpleTable(nameLittle, 2, Table::New,
                                    TableLock(TableLock::AutoLocking),
                                    Table::LittleEndian);
        AlwaysAssertExit(tab.endianFormat() == Table::LittleEndian);
        tab.flush();
    }
    {
        Table tab(nameLittle, Table::Old);
        AlwaysAssertExit(tab.endianFormat() == Table::LittleEndian);
    }

    // LocalEndian
    {
        Table tab = makeSimpleTable(nameLocal, 2, Table::New,
                                    TableLock(TableLock::AutoLocking),
                                    Table::LocalEndian);
        // LocalEndian resolves to the machine's native endian format
        if (HostInfo::bigEndian()) {
            AlwaysAssertExit(tab.endianFormat() == Table::BigEndian);
        } else {
            AlwaysAssertExit(tab.endianFormat() == Table::LittleEndian);
        }
        tab.flush();
    }

    deleteIfExists(nameBig);
    deleteIfExists(nameLittle);
    deleteIfExists(nameLocal);
}

// -----------------------------------------------------------------------
//  Test: getLayout
// -----------------------------------------------------------------------
void testGetLayout()
{
    cout << "testGetLayout" << endl;
    String name = uniqueName("tPTCov_layout");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);
        tab.flush();
    }
    // Use TableUtil::getLayout to read just the schema without fully opening
    {
        TableDesc desc;
        rownr_t nrow = TableUtil::getLayout(desc, name);
        AlwaysAssertExit(nrow == 5);
        AlwaysAssertExit(desc.ncolumn() == 3);
        AlwaysAssertExit(desc.isColumn("IntCol"));
        AlwaysAssertExit(desc.isColumn("DoubleCol"));
        AlwaysAssertExit(desc.isColumn("StringCol"));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: isMultiUsed
// -----------------------------------------------------------------------
void testIsMultiUsed()
{
    cout << "testIsMultiUsed" << endl;
    String name = uniqueName("tPTCov_multiused");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);
        tab.flush();

        // A single-process open should NOT be multi-used
        // (isMultiUsed checks file locks from other processes)
        AlwaysAssertExit(!tab.isMultiUsed(False));

        // With subtable checking
        AlwaysAssertExit(!tab.isMultiUsed(True));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: hasDataChanged (exercises getModifyCounter internally)
// -----------------------------------------------------------------------
void testHasDataChanged()
{
    cout << "testHasDataChanged" << endl;
    String name = uniqueName("tPTCov_datachanged");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);

        // First call initializes internal counter; returns True if
        // there have been changes since last call.
        Bool changed = tab.hasDataChanged();
        // After creation, the data might be considered changed (first call)
        (void)changed;

        // Flush and call again -- should now be False since no further changes
        tab.flush();
        changed = tab.hasDataChanged();
        // The counter was just written; might or might not report changed.
        // What matters is that the path is exercised without crashing.

        // Modify data, flush, then check
        ScalarColumn<Int> intCol(tab, "IntCol");
        intCol.put(0, 777);
        tab.flush();
        changed = tab.hasDataChanged();
        // After a flush with modifications, hasDataChanged should return True
        AlwaysAssertExit(changed);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: keywordSet vs rwKeywordSet
// -----------------------------------------------------------------------
void testKeywordSets()
{
    cout << "testKeywordSets" << endl;
    String name = uniqueName("tPTCov_kwsets");
    deleteIfExists(name);
    {
        // Create table and add keywords via rwKeywordSet
        {
            Table tab = makeSimpleTable(name, 3);
            tab.rwKeywordSet().define("TestKey", Int(42));
            tab.rwKeywordSet().define("TestStr", String("hello"));
            tab.flush();
        }

        // Open read-only and verify via keywordSet (read-only access)
        {
            Table tab(name, Table::Old);
            const TableRecord& kw = tab.keywordSet();
            AlwaysAssertExit(kw.asInt("TestKey") == 42);
            AlwaysAssertExit(kw.asString("TestStr") == "hello");
        }

        // Open for update and verify via rwKeywordSet (read-write access)
        {
            Table tab(name, Table::Update);
            TableRecord& kw = tab.rwKeywordSet();
            AlwaysAssertExit(kw.asInt("TestKey") == 42);
            kw.define("TestKey", Int(100));
            AlwaysAssertExit(kw.asInt("TestKey") == 100);
            tab.flush();
        }

        // Verify updated keyword persisted
        {
            Table tab(name, Table::Old);
            AlwaysAssertExit(tab.keywordSet().asInt("TestKey") == 100);
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: renameHypercolumn
// -----------------------------------------------------------------------
void testRenameHypercolumn()
{
    cout << "testRenameHypercolumn" << endl;
    String name = uniqueName("tPTCov_renhc");
    deleteIfExists(name);
    {
        // Create a table with a hypercolumn definition
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data",
                                             IPosition(1, 8),
                                             ColumnDesc::FixedShape));
        td.defineHypercolumn("OrigHC", 2,
                             Vector<String>(1, "Data"));

        SetupNewTable newtab(name, td, Table::New);
        TiledColumnStMan tsm("TiledSM", IPosition(2, 8, 4));
        newtab.bindColumn("Data", tsm);
        Table tab(newtab, 4);

        // Verify the original hypercolumn exists
        AlwaysAssertExit(tab.tableDesc().isHypercolumn("OrigHC"));

        // Rename the hypercolumn
        tab.renameHypercolumn("NewHC", "OrigHC");

        // Verify the rename
        AlwaysAssertExit(tab.tableDesc().isHypercolumn("NewHC"));
        AlwaysAssertExit(!tab.tableDesc().isHypercolumn("OrigHC"));

        tab.flush();
    }
    // Verify persistence
    {
        Table tab(name, Table::Old);
        AlwaysAssertExit(tab.tableDesc().isHypercolumn("NewHC"));
        AlwaysAssertExit(!tab.tableDesc().isHypercolumn("OrigHC"));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: addRow with initialize
// -----------------------------------------------------------------------
void testAddRowWithInitialize()
{
    cout << "testAddRowWithInitialize" << endl;
    String name = uniqueName("tPTCov_addrow");
    deleteIfExists(name);
    {
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("IntCol"));
        td.addColumn(ScalarColumnDesc<Double>("DoubleCol"));

        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 0);
        AlwaysAssertExit(tab.nrow() == 0);

        // Add rows with initialize=True
        tab.addRow(3, True);
        AlwaysAssertExit(tab.nrow() == 3);

        // Add rows with initialize=False
        tab.addRow(2, False);
        AlwaysAssertExit(tab.nrow() == 5);

        // The initialized rows should be readable
        ScalarColumn<Int> intCol(tab, "IntCol");
        for (uInt i = 0; i < 3; i++) {
            Int val = intCol(i);
            (void)val;  // just verify no crash
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: isWritable for different open modes
// -----------------------------------------------------------------------
void testIsWritable()
{
    cout << "testIsWritable" << endl;
    String name = uniqueName("tPTCov_writable");
    deleteIfExists(name);
    {
        // New -> writable
        {
            Table tab = makeSimpleTable(name, 3);
            AlwaysAssertExit(tab.isWritable());
            tab.flush();
        }

        // Old -> not writable
        {
            Table tab(name, Table::Old);
            AlwaysAssertExit(!tab.isWritable());
        }

        // Update -> writable
        {
            Table tab(name, Table::Update);
            AlwaysAssertExit(tab.isWritable());
        }
    }

    // Scratch -> writable
    {
        String scrName = uniqueName("tPTCov_writscr");
        deleteIfExists(scrName);
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("col"));
        SetupNewTable newtab(scrName, td, Table::Scratch);
        Table tab(newtab, 1);
        AlwaysAssertExit(tab.isWritable());
    }

    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: storageOption
// -----------------------------------------------------------------------
void testStorageOption()
{
    cout << "testStorageOption" << endl;
    String name = uniqueName("tPTCov_stopt");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);
        // Just verify we can call storageOption without crashing
        const StorageOption& sopt = tab.storageOption();
        StorageOption::Option o = sopt.option();
        (void)o;
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: lock / unlock / hasLock
// -----------------------------------------------------------------------
void testLocking()
{
    cout << "testLocking" << endl;
    String name = uniqueName("tPTCov_lock");
    deleteIfExists(name);
    {
        // Create table with AutoLocking (default) and close
        {
            Table tab = makeSimpleTable(name, 3);
            tab.flush();
        }

        // Reopen with UserNoReadLocking so opening does not require a lock,
        // but writing still needs explicit lock/unlock.
        {
            Table tab(name, TableLock(TableLock::UserNoReadLocking),
                      Table::Update);

            // With UserNoReadLocking, no lock is held initially for reading
            AlwaysAssertExit(!tab.hasLock(FileLocker::Write));

            // Acquire a write lock
            Bool gotLock = tab.lock(FileLocker::Write, 1);
            AlwaysAssertExit(gotLock);
            AlwaysAssertExit(tab.hasLock(FileLocker::Write));
            AlwaysAssertExit(tab.hasLock(FileLocker::Read));

            // Unlock
            tab.unlock();
            AlwaysAssertExit(!tab.hasLock(FileLocker::Write));

            // Acquire a read lock
            gotLock = tab.lock(FileLocker::Read, 1);
            AlwaysAssertExit(gotLock);
            AlwaysAssertExit(tab.hasLock(FileLocker::Read));

            tab.unlock();
        }

        // Test with PermanentLocking -- lock is always held
        {
            Table tab(name, TableLock(TableLock::PermanentLocking),
                      Table::Update);
            AlwaysAssertExit(tab.hasLock(FileLocker::Write));

            // unlock() should be a no-op for PermanentLocking
            tab.unlock();
            AlwaysAssertExit(tab.hasLock(FileLocker::Write));
        }

        // Test with AutoLocking -- lock acquired automatically on data access
        {
            Table tab(name, TableLock(TableLock::AutoLocking),
                      Table::Update);
            // Trigger a write which forces automatic lock acquisition
            ScalarColumn<Int> intCol(tab, "IntCol");
            intCol.put(0, 42);
            AlwaysAssertExit(tab.hasLock(FileLocker::Write));
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: flush with recursive (subtables)
// -----------------------------------------------------------------------
void testFlushRecursive()
{
    cout << "testFlushRecursive" << endl;
    String mainName = uniqueName("tPTCov_flushmain");
    String subName = uniqueName("tPTCov_flushsub");
    deleteIfExists(mainName);
    deleteIfExists(subName);
    {
        // Create a subtable
        Table subTab = makeSimpleTable(subName, 2);
        subTab.flush();

        // Create the main table and add the subtable as a keyword
        Table mainTab = makeSimpleTable(mainName, 3);
        mainTab.rwKeywordSet().defineTable("SUBTABLE", subTab);

        // Flush recursively -- should also flush the subtable
        mainTab.flush(False, True);

        // Verify the subtable keyword is accessible
        Table subFromMain = mainTab.keywordSet().asTable("SUBTABLE");
        AlwaysAssertExit(subFromMain.nrow() == 2);
    }
    // Verify both tables persist
    {
        Table mainTab(mainName, Table::Old);
        AlwaysAssertExit(mainTab.nrow() == 3);
        Table subFromMain = mainTab.keywordSet().asTable("SUBTABLE");
        AlwaysAssertExit(subFromMain.nrow() == 2);
    }
    deleteIfExists(mainName);
    deleteIfExists(subName);
}

// -----------------------------------------------------------------------
//  Test: dataManagerInfo and actualTableDesc
// -----------------------------------------------------------------------
void testTableDescAndDMInfo()
{
    cout << "testTableDescAndDMInfo" << endl;
    String name = uniqueName("tPTCov_dmi");
    deleteIfExists(name);
    {
        Table tab = makeTiledTable(name, 4);

        // actualTableDesc should reflect the runtime table description
        TableDesc atd = tab.actualTableDesc();
        AlwaysAssertExit(atd.ncolumn() >= 2);
        AlwaysAssertExit(atd.isColumn("IntCol"));
        AlwaysAssertExit(atd.isColumn("TiledArr"));

        // dataManagerInfo should include at least the TiledColumnStMan
        Record dmi = tab.dataManagerInfo();
        AlwaysAssertExit(dmi.nfields() > 0);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: Table with different NewNoReplace safety
// -----------------------------------------------------------------------
void testNewNoReplace()
{
    cout << "testNewNoReplace" << endl;
    String name = uniqueName("tPTCov_nnr");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 2);
        tab.flush();
    }
    // Creating a table with NewNoReplace on an existing table should throw
    expectThrows([&]() {
        makeSimpleTable(name, 1, Table::NewNoReplace);
    });
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: Table multiple opens (table cache check)
// -----------------------------------------------------------------------
void testMultipleOpens()
{
    cout << "testMultipleOpens" << endl;
    String name = uniqueName("tPTCov_multiopen");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);
        tab.flush();

        // Open the same table again read-only (uses table cache)
        Table tab2(name, Table::Old);
        AlwaysAssertExit(tab2.nrow() == 3);

        // Both should share the same underlying storage (from cache)
        AlwaysAssertExit(tab.tableName() == tab2.tableName());
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: findDataManager
// -----------------------------------------------------------------------
void testFindDataManager()
{
    cout << "testFindDataManager" << endl;
    String name = uniqueName("tPTCov_finddm");
    deleteIfExists(name);
    {
        Table tab = makeTiledTable(name, 4);

        // Find by column name
        DataManager* dm = tab.findDataManager("TiledArr", True);
        AlwaysAssertExit(dm != nullptr);

        // Find by DM name
        DataManager* dm2 = tab.findDataManager("TiledSM", False);
        AlwaysAssertExit(dm2 != nullptr);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: canAddRow, canRemoveRow, canRemoveColumn, canRenameColumn
// -----------------------------------------------------------------------
void testCanOperations()
{
    cout << "testCanOperations" << endl;
    String name = uniqueName("tPTCov_canops");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);

        // StandardStMan supports row add/remove
        AlwaysAssertExit(tab.canAddRow());
        AlwaysAssertExit(tab.canRemoveRow());

        // Can remove existing column
        Vector<String> cols(1, "IntCol");
        AlwaysAssertExit(tab.canRemoveColumn(cols));

        // Cannot remove non-existent column
        Vector<String> badcols(1, "NoSuchCol");
        AlwaysAssertExit(!tab.canRemoveColumn(badcols));

        // canRenameColumn
        AlwaysAssertExit(tab.canRenameColumn("IntCol"));
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: lockOptions
// -----------------------------------------------------------------------
void testLockOptions()
{
    cout << "testLockOptions" << endl;
    String name = uniqueName("tPTCov_lockopt");
    deleteIfExists(name);
    {
        // Create with AutoLocking
        {
            Table tab = makeSimpleTable(name, 2,
                                        Table::New,
                                        TableLock(TableLock::AutoLocking));
            const TableLock& lopt = tab.lockOptions();
            AlwaysAssertExit(!lopt.isPermanent());
            tab.flush();
        }

        // Reopen with PermanentLocking
        {
            Table tab(name, TableLock(TableLock::PermanentLocking),
                      Table::Update);
            const TableLock& lopt = tab.lockOptions();
            AlwaysAssertExit(lopt.isPermanent());
        }
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: addColumn, removeColumn, renameColumn
// -----------------------------------------------------------------------
void testColumnOperations()
{
    cout << "testColumnOperations" << endl;
    String name = uniqueName("tPTCov_colops");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);

        // addColumn
        tab.addColumn(ScalarColumnDesc<Float>("NewFloat"));
        AlwaysAssertExit(tab.tableDesc().isColumn("NewFloat"));

        // Write and read back the new column
        ScalarColumn<Float> fCol(tab, "NewFloat");
        fCol.put(0, 1.5f);
        AlwaysAssertExit(fCol(0) == 1.5f);

        // renameColumn
        tab.renameColumn("RenamedFloat", "NewFloat");
        AlwaysAssertExit(tab.tableDesc().isColumn("RenamedFloat"));
        AlwaysAssertExit(!tab.tableDesc().isColumn("NewFloat"));

        // removeColumn
        Vector<String> toRemove(1, "RenamedFloat");
        tab.removeColumn(toRemove);
        AlwaysAssertExit(!tab.tableDesc().isColumn("RenamedFloat"));

        // Attempt to remove non-existent column should throw
        expectThrows([&]() {
            Vector<String> bad(1, "NoSuchCol");
            tab.removeColumn(bad);
        });

        tab.flush();
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: removeRow
// -----------------------------------------------------------------------
void testRemoveRow()
{
    cout << "testRemoveRow" << endl;
    String name = uniqueName("tPTCov_rmrow");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 5);
        AlwaysAssertExit(tab.nrow() == 5);

        // Remove a single row
        tab.removeRow(2);
        AlwaysAssertExit(tab.nrow() == 4);

        // Remove multiple rows via vector
        Vector<rownr_t> rows(2);
        rows(0) = 0;
        rows(1) = 3;
        tab.removeRow(rows);
        AlwaysAssertExit(tab.nrow() == 2);
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: checkWritable (via write on read-only table)
// -----------------------------------------------------------------------
void testCheckWritable()
{
    cout << "testCheckWritable" << endl;
    String name = uniqueName("tPTCov_chkwr");
    deleteIfExists(name);
    {
        Table tab = makeSimpleTable(name, 3);
        tab.flush();
    }
    {
        Table tab(name, Table::Old);
        AlwaysAssertExit(!tab.isWritable());

        // Attempting to add a row on a read-only table should throw
        expectThrows([&]() {
            tab.addRow();
        });

        // Attempting to add a column on a read-only table should throw
        expectThrows([&]() {
            tab.addColumn(ScalarColumnDesc<Int>("BadCol"));
        });

        // Attempting to remove a column on a read-only table should throw
        expectThrows([&]() {
            Vector<String> cols(1, "IntCol");
            tab.removeColumn(cols);
        });

        // Attempting to rename a column on a read-only table should throw
        expectThrows([&]() {
            tab.renameColumn("NewName", "IntCol");
        });
    }
    deleteIfExists(name);
}

// -----------------------------------------------------------------------
//  Test: table with initial rows and initialize flag
// -----------------------------------------------------------------------
void testCreateWithInitialize()
{
    cout << "testCreateWithInitialize" << endl;
    String name = uniqueName("tPTCov_initcreate");
    deleteIfExists(name);
    {
        // Create table with initialize=True via constructor
        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("IntCol"));
        td.addColumn(ScalarColumnDesc<Double>("DblCol"));
        SetupNewTable newtab(name, td, Table::New);
        Table tab(newtab, 5, True);  // 5 rows, initialize=True
        AlwaysAssertExit(tab.nrow() == 5);

        // Verify rows are readable (initialized to default values)
        ScalarColumn<Int> intCol(tab, "IntCol");
        ScalarColumn<Double> dblCol(tab, "DblCol");
        for (uInt i = 0; i < 5; i++) {
            Int iv = intCol(i);
            Double dv = dblCol(i);
            (void)iv;
            (void)dv;
        }
    }
    deleteIfExists(name);
}

}  // namespace

int main()
{
    try {
        testChangeTiledDataOnly();
        testReopenRW();
        testTableOptions();
        testEndianFormat();
        testGetLayout();
        testIsMultiUsed();
        testHasDataChanged();
        testKeywordSets();
        testRenameHypercolumn();
        testAddRowWithInitialize();
        testIsWritable();
        testStorageOption();
        testLocking();
        testFlushRecursive();
        testTableDescAndDMInfo();
        testNewNoReplace();
        testMultipleOpens();
        testFindDataManager();
        testCanOperations();
        testLockOptions();
        testColumnOperations();
        testRemoveRow();
        testCheckWritable();
        testCreateWithInitialize();
    } catch (const std::exception& e) {
        cerr << "FAIL: " << e.what() << endl;
        return 1;
    }
    cout << "OK" << endl;
    return 0;
}
