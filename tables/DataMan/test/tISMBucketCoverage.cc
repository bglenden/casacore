//# tISMBucketCoverage.cc: Characterization coverage for ISMBucket
//# split/merge/shift logic exercised through the public IncrementalStMan API.
//#
//# Tranche J of the casacore modernization plan.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/DataMan/IncrementalStMan.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/Exceptions/Error.h>

#include <iostream>
#include <unistd.h>

using namespace casacore;
using namespace std;

namespace {

String uniqueName(const String& base) {
    return base + "_" + String::toString(Int(getpid()));
}

void deleteIfExists(const String& name) {
    if (Table::isReadable(name)) {
        TableUtil::deleteTable(name, True);
    }
}

// =========================================================================
// 1. testSequentialSplits -- Force the simpleSplit path
// =========================================================================
// Writing unique values sequentially to a small-bucket ISM forces
// bucket splits via the simpleSplit path (appending at end of last bucket).

void testSequentialSplits()
{
    cout << "testSequentialSplits" << endl;
    String tabName = uniqueName("tISMBCov_SeqSplit");
    deleteIfExists(tabName);

    const uInt nRows = 100;
    const uInt bucketSize = 128;

    // Write phase: create table, fill with unique values.
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, Int(i * 7 + 3));  // unique per row
        }

        // Verify before close.
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(col(i) == Int(i * 7 + 3));
        }
    }

    // Reopen and verify -- exercises read callbacks on multiple buckets.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(col(i) == Int(i * 7 + 3));
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 2. testMidBucketSplit -- Force the general getSplit path
// =========================================================================
// First fill sequentially (creating multiple buckets via simpleSplit),
// then modify interior rows, forcing the general split (not simpleSplit).

void testMidBucketSplit()
{
    cout << "testMidBucketSplit" << endl;
    String tabName = uniqueName("tISMBCov_MidSplit");
    deleteIfExists(tabName);

    const uInt nRows = 50;
    const uInt bucketSize = 128;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");
        // Sequential fill with unique values.
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, Int(i * 10));
        }

        // Now go back and modify interior rows.
        // This modifies rows in non-last buckets, triggering the general
        // split path (getSplit) rather than simpleSplit.
        for (uInt i = 5; i < nRows; i += 5) {
            col.put(i, Int(i * 10 + 999));
        }

        // Verify all values.
        for (uInt i = 0; i < nRows; ++i) {
            Int expected = (i % 5 == 0 && i >= 5)
                               ? Int(i * 10 + 999)
                               : Int(i * 10);
            AlwaysAssertExit(col(i) == expected);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            Int expected = (i % 5 == 0 && i >= 5)
                               ? Int(i * 10 + 999)
                               : Int(i * 10);
            AlwaysAssertExit(col(i) == expected);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 3. testStringSplits -- Variable-length data path
// =========================================================================
// Strings are variable-length in ISM. This exercises getLength with
// fixedLength==0, removeData with variable sizes, and replaceData
// size-change paths.

void testStringSplits()
{
    cout << "testStringSplits" << endl;
    String tabName = uniqueName("tISMBCov_StrSplit");
    deleteIfExists(tabName);

    const uInt nRows = 60;
    const uInt bucketSize = 256;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<String>("str"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<String> col(tab, "str");
        // Write strings of varying lengths to force variable-length
        // bucket operations with frequent splits.
        for (uInt i = 0; i < nRows; ++i) {
            String val = "row_" + String::toString(i) + "_";
            // Make some strings much longer to vary the data sizes.
            for (uInt j = 0; j < (i % 7); ++j) {
                val += "padding_";
            }
            col.put(i, val);
        }

        // Verify all strings.
        for (uInt i = 0; i < nRows; ++i) {
            String expected = "row_" + String::toString(i) + "_";
            for (uInt j = 0; j < (i % 7); ++j) {
                expected += "padding_";
            }
            AlwaysAssertExit(col(i) == expected);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<String> col(tab, "str");
        for (uInt i = 0; i < nRows; ++i) {
            String expected = "row_" + String::toString(i) + "_";
            for (uInt j = 0; j < (i % 7); ++j) {
                expected += "padding_";
            }
            AlwaysAssertExit(col(i) == expected);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 4. testReplaceData -- Same-size and different-size replacement
// =========================================================================
// For Int columns: replaceData with same fixedLength (memcpy path).
// For String columns: replaceData with different lengths
// (removeData + insertData path).

void testReplaceData()
{
    cout << "testReplaceData" << endl;
    String tabName = uniqueName("tISMBCov_Replace");
    deleteIfExists(tabName);

    const uInt nRows = 40;
    const uInt bucketSize = 256;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("ival"));
        td.addColumn(ScalarColumnDesc<String>("sval"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> icol(tab, "ival");
        ScalarColumn<String> scol(tab, "sval");

        // Write initial unique values.
        for (uInt i = 0; i < nRows; ++i) {
            icol.put(i, Int(i * 100));
            scol.put(i, "initial_" + String::toString(i));
        }

        // Replace Int values (same fixedLength -> memcpy path).
        for (uInt i = 0; i < nRows; i += 3) {
            icol.put(i, Int(i * 100 + 50));
        }

        // Replace String values with different lengths
        // (triggers removeData + insertData path).
        for (uInt i = 1; i < nRows; i += 4) {
            String newVal = "replaced_with_longer_string_" + String::toString(i);
            scol.put(i, newVal);
        }

        // Verify all values.
        for (uInt i = 0; i < nRows; ++i) {
            Int expectedInt = (i % 3 == 0) ? Int(i * 100 + 50) : Int(i * 100);
            AlwaysAssertExit(icol(i) == expectedInt);

            String expectedStr;
            if (i >= 1 && (i - 1) % 4 == 0) {
                expectedStr = "replaced_with_longer_string_" + String::toString(i);
            } else {
                expectedStr = "initial_" + String::toString(i);
            }
            AlwaysAssertExit(scol(i) == expectedStr);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int> icol(tab, "ival");
        ScalarColumn<String> scol(tab, "sval");
        for (uInt i = 0; i < nRows; ++i) {
            Int expectedInt = (i % 3 == 0) ? Int(i * 100 + 50) : Int(i * 100);
            AlwaysAssertExit(icol(i) == expectedInt);

            String expectedStr;
            if (i >= 1 && (i - 1) % 4 == 0) {
                expectedStr = "replaced_with_longer_string_" + String::toString(i);
            } else {
                expectedStr = "initial_" + String::toString(i);
            }
            AlwaysAssertExit(scol(i) == expectedStr);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 5. testShiftLeftMerge -- Exercise shiftLeft with nr > 1
// =========================================================================
// ISM merges entries when a put makes a value equal to its neighbor.
// Writing alternating A, B, A, B... then overwriting B with A triggers
// the "equal to neighbor" optimization which calls shiftLeft.
// When both prev and next match, shiftLeft is called with nr=2.

void testShiftLeftMerge()
{
    cout << "testShiftLeftMerge" << endl;
    String tabName = uniqueName("tISMBCov_Shift");
    deleteIfExists(tabName);

    const uInt nRows = 30;
    const uInt bucketSize = 256;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");

        // Write alternating values: 100, 200, 100, 200, ...
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, (i % 2 == 0) ? 100 : 200);
        }

        // Verify alternating pattern.
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(col(i) == ((i % 2 == 0) ? 100 : 200));
        }

        // Now overwrite some B (200) values with A (100).
        // This causes ISM to detect equal neighbors and call shiftLeft.
        // When row i has value 200 and both row i-1 and row i+1 have 100,
        // changing row i to 100 triggers shiftLeft with nr=2 (both the
        // entry for row i and the re-entry for the next interval merge).
        col.put(1, 100);   // row 1: was 200, now matches neighbors
        col.put(5, 100);   // row 5: same thing
        col.put(9, 100);   // row 9: same thing

        // Verify updated values.
        for (uInt i = 0; i < nRows; ++i) {
            Int expected;
            if (i == 1 || i == 5 || i == 9) {
                expected = 100;
            } else {
                expected = (i % 2 == 0) ? 100 : 200;
            }
            AlwaysAssertExit(col(i) == expected);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            Int expected;
            if (i == 1 || i == 5 || i == 9) {
                expected = 100;
            } else {
                expected = (i % 2 == 0) ? 100 : 200;
            }
            AlwaysAssertExit(col(i) == expected);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 6. testMultipleColumns -- Multiple columns per ISM
// =========================================================================
// Multiple columns on one ISM with small bucket forces bucket splits
// with multi-column index management (the bucket index tracks all columns).

void testMultipleColumns()
{
    cout << "testMultipleColumns" << endl;
    String tabName = uniqueName("tISMBCov_MultiCol");
    deleteIfExists(tabName);

    const uInt nRows = 60;
    const uInt bucketSize = 192;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("iCol"));
        td.addColumn(ScalarColumnDesc<Float>("fCol"));
        td.addColumn(ScalarColumnDesc<String>("sCol"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int>    iCol(tab, "iCol");
        ScalarColumn<Float>  fCol(tab, "fCol");
        ScalarColumn<String> sCol(tab, "sCol");

        // Write unique values to all columns.
        for (uInt i = 0; i < nRows; ++i) {
            iCol.put(i, Int(i * 11));
            fCol.put(i, Float(i) * 1.5f + 0.1f);
            sCol.put(i, "mc_" + String::toString(i));
        }

        // Verify.
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(iCol(i) == Int(i * 11));
            AlwaysAssertExit(fCol(i) == Float(i) * 1.5f + 0.1f);
            AlwaysAssertExit(sCol(i) == "mc_" + String::toString(i));
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int>    iCol(tab, "iCol");
        ScalarColumn<Float>  fCol(tab, "fCol");
        ScalarColumn<String> sCol(tab, "sCol");

        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(iCol(i) == Int(i * 11));
            AlwaysAssertExit(fCol(i) == Float(i) * 1.5f + 0.1f);
            AlwaysAssertExit(sCol(i) == "mc_" + String::toString(i));
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 7. testAddRemoveRows -- Row removal through ISM
// =========================================================================
// Removing rows from a populated table exercises the ISMColumn::remove
// path which calls shiftLeft on bucket entries and adjusts row indices.

void testAddRemoveRows()
{
    cout << "testAddRemoveRows" << endl;
    String tabName = uniqueName("tISMBCov_AddRm");
    deleteIfExists(tabName);

    const uInt nRows = 50;
    const uInt bucketSize = 128;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, Int(i * 5));
        }

        // Remove rows from the middle, exercising removeData paths
        // within buckets. Remove from high indices first to avoid shifting.
        tab.removeRow(40);
        tab.removeRow(30);
        tab.removeRow(20);
        tab.removeRow(10);
        tab.removeRow(0);
        AlwaysAssertExit(tab.nrow() == 45);

        // Verify remaining rows have the expected values.
        // Original rows removed: 0, 10, 20, 30, 40
        // Remaining original indices: 1-9, 11-19, 21-29, 31-39, 41-49
        // After each removeRow, the table renumbers.
        // We build expected values by tracking what remains.
        Vector<Int> expected(45);
        uInt k = 0;
        for (uInt i = 0; i < nRows; ++i) {
            if (i != 0 && i != 10 && i != 20 && i != 30 && i != 40) {
                expected[k++] = Int(i * 5);
            }
        }
        AlwaysAssertExit(k == 45);

        for (uInt i = 0; i < 45; ++i) {
            AlwaysAssertExit(col(i) == expected[i]);
        }
    }

    // Reopen and verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == 45);
        ScalarColumn<Int> col(tab, "val");

        Vector<Int> expected(45);
        uInt k = 0;
        for (uInt i = 0; i < nRows; ++i) {
            if (i != 0 && i != 10 && i != 20 && i != 30 && i != 40) {
                expected[k++] = Int(i * 5);
            }
        }
        for (uInt i = 0; i < 45; ++i) {
            AlwaysAssertExit(col(i) == expected[i]);
        }
    }
    deleteIfExists(tabName);
}

// =========================================================================
// 8. testLargeDataset -- Stress test with many splits
// =========================================================================
// Write 1000+ rows with small bucket size to force many (50+) bucket
// splits. Verify first, middle, and last values. Close, reopen, verify
// again (exercises read callbacks on many buckets).

void testLargeDataset()
{
    cout << "testLargeDataset" << endl;
    String tabName = uniqueName("tISMBCov_Large");
    deleteIfExists(tabName);

    const uInt nRows = 1200;
    const uInt bucketSize = 128;

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Int>("val"));

        SetupNewTable newtab(tabName, td, Table::New);
        IncrementalStMan ism("ISM", bucketSize, False);
        newtab.bindAll(ism);
        Table tab(newtab, nRows);

        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            col.put(i, Int(i * 3 + 1));
        }

        // Spot-check first, middle, and last.
        AlwaysAssertExit(col(0) == 1);
        AlwaysAssertExit(col(nRows / 2) == Int((nRows / 2) * 3 + 1));
        AlwaysAssertExit(col(nRows - 1) == Int((nRows - 1) * 3 + 1));
    }

    // Reopen and do a full verify.
    {
        Table tab(tabName, Table::Old);
        AlwaysAssertExit(tab.nrow() == nRows);
        ScalarColumn<Int> col(tab, "val");
        for (uInt i = 0; i < nRows; ++i) {
            AlwaysAssertExit(col(i) == Int(i * 3 + 1));
        }
    }
    deleteIfExists(tabName);
}

}  // anonymous namespace


int main()
{
    try {
        testSequentialSplits();
        testMidBucketSplit();
        testStringSplits();
        testReplaceData();
        testShiftLeftMerge();
        testMultipleColumns();
        testAddRemoveRows();
        testLargeDataset();

        cout << "All ISMBucket coverage tests passed." << endl;
        return 0;
    } catch (const AipsError& e) {
        cerr << "FAIL: " << e.what() << endl;
        return 1;
    }
}
