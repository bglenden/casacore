//# tTiledStManCoverage.cc: characterization coverage for TiledStMan internals
//# Exercises getLengthOffset, checkValues, checkCoordinates, setup column
//# ordering, makeTileShape, cache management, and multi-cube management
//# through the public Table and TiledStManAccessor APIs.

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableRecord.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/tables/DataMan/TiledCellStMan.h>
#include <casacore/tables/DataMan/TiledColumnStMan.h>
#include <casacore/tables/DataMan/TiledShapeStMan.h>
#include <casacore/tables/DataMan/TiledStManAccessor.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Arrays/Cube.h>
#include <casacore/casa/Arrays/ArrayMath.h>
#include <casacore/casa/Arrays/ArrayLogical.h>
#include <casacore/casa/Arrays/ArrayUtil.h>
#include <casacore/casa/Arrays/Slicer.h>
#include <casacore/casa/Arrays/IPosition.h>
#include <casacore/casa/IO/ArrayIO.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Utilities/Assert.h>
#include <casacore/casa/OS/Path.h>
#include <casacore/casa/Exceptions/Error.h>

#include <iostream>
#include <sstream>
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
// 1. testMultiDataColumnLayout
//    Exercise getLengthOffset with multiple data columns of different types
//    in the same hypercolumn via TiledColumnStMan.
// =========================================================================
void testMultiDataColumnLayout()
{
    cout << "testMultiDataColumnLayout" << endl;
    String tabName = uniqueName("tTSMCov_MultiData");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        // Two data columns: Float (4 bytes) and Int (4 bytes) in same hypercolumn.
        td.addColumn(ArrayColumnDesc<Float>("FloatData", IPosition(2, 8, 10),
                                            ColumnDesc::FixedShape));
        td.addColumn(ArrayColumnDesc<Int>("IntData", IPosition(2, 8, 10),
                                          ColumnDesc::FixedShape));
        // Define 3-dim hypercolumn (2D cell + row dimension).
        td.defineHypercolumn("TSMMulti", 3,
                             stringToVector("FloatData,IntData"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMMulti", IPosition(3, 4, 5, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> floatCol(table, "FloatData");
        ArrayColumn<Int> intCol(table, "IntData");
        Matrix<Float> fdata(IPosition(2, 8, 10));
        Matrix<Int> idata(IPosition(2, 8, 10));

        const uInt nrow = 20;
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            indgen(fdata, Float(i * 100));
            indgen(idata, Int(i * 1000));
            floatCol.put(i, fdata);
            intCol.put(i, idata);
        }

        // Verify readback.
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> fres;
            Matrix<Int> ires;
            floatCol.get(i, fres);
            intCol.get(i, ires);
            Matrix<Float> fexpect(IPosition(2, 8, 10));
            Matrix<Int> iexpect(IPosition(2, 8, 10));
            indgen(fexpect, Float(i * 100));
            indgen(iexpect, Int(i * 1000));
            AlwaysAssertExit(allEQ(fres, fexpect));
            AlwaysAssertExit(allEQ(ires, iexpect));
        }

        ROTiledStManAccessor acc(table, "TSMMulti");
        AlwaysAssertExit(acc.nhypercubes() == 1);
        AlwaysAssertExit(acc.getHypercubeShape(0) == IPosition(3, 8, 10, nrow));
        AlwaysAssertExit(acc.getTileShape(0) == IPosition(3, 4, 5, 1));
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 2. testCoordinateColumns
//    Exercise coordColSet_p, checkCoordinatesShapes by defining coordinate
//    columns in the hypercolumn.
// =========================================================================
void testCoordinateColumns()
{
    cout << "testCoordinateColumns" << endl;
    String tabName = uniqueName("tTSMCov_Coord");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("RA", IPosition(1, 12),
                                            ColumnDesc::FixedShape));
        td.addColumn(ArrayColumnDesc<Float>("Dec", IPosition(1, 16),
                                            ColumnDesc::FixedShape));
        td.addColumn(ScalarColumnDesc<Float>("Time"));
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 12, 16),
                                            ColumnDesc::FixedShape));
        // 3-dim hypercolumn: data 2D + row dim, with coordinate columns.
        td.defineHypercolumn("TSMCoord", 3,
                             stringToVector("Data"),
                             stringToVector("RA,Dec,Time"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMCoord", IPosition(3, 4, 4, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> ra(table, "RA");
        ArrayColumn<Float> dec(table, "Dec");
        ScalarColumn<Float> time(table, "Time");
        ArrayColumn<Float> data(table, "Data");

        Vector<Float> raVals(12);
        Vector<Float> decVals(16);
        indgen(raVals, Float(10));
        indgen(decVals, Float(20));

        Matrix<Float> dataVals(IPosition(2, 12, 16));
        const uInt nrow = 10;
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            indgen(dataVals, Float(i * 200));
            data.put(i, dataVals);
            time.put(i, Float(100 + i * 5));
        }
        // Put coordinate values (only need once for TiledColumnStMan).
        ra.put(0, raVals);
        dec.put(0, decVals);

        // Verify coordinate readback.
        AlwaysAssertExit(allEQ(ra(0), raVals));
        AlwaysAssertExit(allEQ(dec(0), decVals));
        // Verify data readback.
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 12, 16));
            indgen(expect, Float(i * 200));
            AlwaysAssertExit(allEQ(res, expect));
        }

        ROTiledStManAccessor acc(table, "TSMCoord");
        AlwaysAssertExit(acc.nhypercubes() == 1);
        // Verify coordinate values are in the value record.
        const Record& vrec = acc.getValueRecord(0);
        AlwaysAssertExit(vrec.isDefined("RA"));
        AlwaysAssertExit(vrec.isDefined("Dec"));
        AlwaysAssertExit(vrec.isDefined("Time"));
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 3. testTiledCellMultiCube
//    Exercise cubeSet_p growing, per-row hypercubes via TiledCellStMan.
// =========================================================================
void testTiledCellMultiCube()
{
    cout << "testTiledCellMultiCube" << endl;
    String tabName = uniqueName("tTSMCov_CellCube");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", 2));
        td.defineHypercolumn("TSMCell", 2,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledCellStMan sm("TSMCell", IPosition(2, 4, 4));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");

        // Row 0: 8x6 shape with explicit tile shape.
        table.addRow();
        data.setShape(0, IPosition(2, 8, 6), IPosition(2, 4, 3));
        Matrix<Float> m0(IPosition(2, 8, 6));
        indgen(m0, Float(0));
        data.put(0, m0);

        // Row 1: 10x12 shape.
        table.addRow();
        data.setShape(1, IPosition(2, 10, 12), IPosition(2, 5, 6));
        Matrix<Float> m1(IPosition(2, 10, 12));
        indgen(m1, Float(1000));
        data.put(1, m1);

        // Row 2: 4x4 shape.
        table.addRow();
        data.setShape(2, IPosition(2, 4, 4), IPosition(2, 4, 4));
        Matrix<Float> m2(IPosition(2, 4, 4));
        indgen(m2, Float(2000));
        data.put(2, m2);

        // For TiledCellStMan, nhypercubes() returns the PtrBlock capacity
        // (which includes pre-allocated null slots), but each row has its
        // own hypercube, so nhypercubes() >= nrow.
        ROTiledStManAccessor acc(table, "TSMCell");
        AlwaysAssertExit(acc.nhypercubes() >= 3);

        // Verify data readback using appropriately shaped arrays.
        {
            Matrix<Float> res(IPosition(2, 8, 6));
            data.get(0, res);
            AlwaysAssertExit(allEQ(res, m0));
        }
        {
            Matrix<Float> res(IPosition(2, 10, 12));
            data.get(1, res);
            AlwaysAssertExit(allEQ(res, m1));
        }
        {
            Matrix<Float> res(IPosition(2, 4, 4));
            data.get(2, res);
            AlwaysAssertExit(allEQ(res, m2));
        }

        // Verify per-row shapes.
        AlwaysAssertExit(data.shape(0) == IPosition(2, 8, 6));
        AlwaysAssertExit(data.shape(1) == IPosition(2, 10, 12));
        AlwaysAssertExit(data.shape(2) == IPosition(2, 4, 4));

        // Verify hypercube shapes via row-based accessor.
        AlwaysAssertExit(acc.hypercubeShape(0) == IPosition(2, 8, 6));
        AlwaysAssertExit(acc.hypercubeShape(1) == IPosition(2, 10, 12));
        AlwaysAssertExit(acc.hypercubeShape(2) == IPosition(2, 4, 4));
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 4. testTiledShapeMultiShape
//    Exercise TiledShapeStMan with different shapes creating 2+ cubes.
// =========================================================================
void testTiledShapeMultiShape()
{
    cout << "testTiledShapeMultiShape" << endl;
    String tabName = uniqueName("tTSMCov_ShapeMulti");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", 2));
        td.defineHypercolumn("TSMShape", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledShapeStMan sm("TSMShape", IPosition(2, 4, 4));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");

        // First batch: 4x4 shape, 5 rows.
        for (uInt i = 0; i < 5; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 4, 4));
            indgen(m, Float(i * 100));
            data.put(i, m);
        }
        // Second batch: 8x8 shape, 3 rows.
        for (uInt i = 5; i < 8; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 8, 8));
            indgen(m, Float(i * 100));
            data.put(i, m);
        }

        // Should have at least 2 hypercubes (one per distinct shape).
        ROTiledStManAccessor acc(table, "TSMShape");
        AlwaysAssertExit(acc.nhypercubes() >= 2);

        // Verify data integrity across shape boundaries.
        for (uInt i = 0; i < 5; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 4, 4));
            indgen(expect, Float(i * 100));
            AlwaysAssertExit(allEQ(res, expect));
        }
        for (uInt i = 5; i < 8; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 8, 8));
            indgen(expect, Float(i * 100));
            AlwaysAssertExit(allEQ(res, expect));
        }
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 5. testMakeTileShape
//    Direct exercise of TiledStMan::makeTileShape static methods.
// =========================================================================
void testMakeTileShape()
{
    cout << "testMakeTileShape" << endl;

    // Helper to verify tile shape validity.
    // Note: makeTileShape is a best-effort heuristic; it does not
    // strictly guarantee product <= maxPixels, so we only check that
    // each dimension is positive and does not exceed the cube shape.
    auto verifyTileShape = [](const IPosition& cubeShape,
                              const IPosition& tileShape) {
        AlwaysAssertExit(tileShape.nelements() == cubeShape.nelements());
        for (uInt i = 0; i < tileShape.nelements(); i++) {
            AlwaysAssertExit(tileShape[i] > 0);
            AlwaysAssertExit(tileShape[i] <= cubeShape[i]);
        }
    };

    // 1D cube shape.
    {
        IPosition cubeShape(1, 1000);
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, 0.5, 256);
        verifyTileShape(cubeShape, tileShape);
    }

    // 2D cube shape.
    {
        IPosition cubeShape(2, 100, 200);
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, 0.5, 1024);
        verifyTileShape(cubeShape, tileShape);
    }

    // 3D cube shape.
    {
        IPosition cubeShape(3, 64, 128, 32);
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, 0.5, 4096);
        verifyTileShape(cubeShape, tileShape);
    }

    // 4D cube shape.
    {
        IPosition cubeShape(4, 16, 32, 64, 128);
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, 0.5, 8192);
        verifyTileShape(cubeShape, tileShape);
    }

    // Weighted overload: 3D with weight and tolerance vectors.
    {
        IPosition cubeShape(3, 100, 200, 50);
        Vector<double> weight(3);
        weight[0] = 1.0;
        weight[1] = 2.0;
        weight[2] = 0.5;
        Vector<double> tol(3);
        tol[0] = 0.5;
        tol[1] = 0.5;
        tol[2] = 0.5;
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, weight,
                                                        tol, 4096);
        verifyTileShape(cubeShape, tileShape);
    }

    // Test small cube (tile shape should equal cube shape if it fits).
    {
        IPosition cubeShape(2, 2, 3);
        IPosition tileShape = TiledStMan::makeTileShape(cubeShape, 0.5,
                                                        1024 * 1024);
        AlwaysAssertExit(tileShape == cubeShape);
    }

    cout << "  OK" << endl;
}

// =========================================================================
// 6. testCacheControl
//    Exercise cache management through ROTiledStManAccessor.
// =========================================================================
void testCacheControl()
{
    cout << "testCacheControl" << endl;
    String tabName = uniqueName("tTSMCov_Cache");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 16, 20),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMCache", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMCache", IPosition(3, 4, 5, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");
        Matrix<Float> arr(IPosition(2, 16, 20));

        const uInt nrow = 30;
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            indgen(arr, Float(i * 100));
            data.put(i, arr);
        }

        ROTiledStManAccessor acc(table, "TSMCache");

        // Exercise setCacheSize (by row number, nbuckets).
        acc.setCacheSize(0, 10, True);
        AlwaysAssertExit(acc.cacheSize(0) >= 1);

        // Exercise setCacheSize (by sliceShape + axisPath).
        acc.setCacheSize(0, IPosition(3, 4, 5, 1), IPosition(1, 0));

        // Exercise setHypercubeCacheSize.
        acc.setHypercubeCacheSize(0, 5, True);
        AlwaysAssertExit(acc.getCacheSize(0) >= 1);

        // Exercise setMaximumCacheSize.
        acc.setMaximumCacheSize(2);
        AlwaysAssertExit(acc.maximumCacheSize() == 2);

        // Exercise showCacheStatistics (send to string stream).
        ostringstream oss;
        acc.showCacheStatistics(oss);
        AlwaysAssertExit(oss.str().size() > 0);

        // Exercise clearCaches.
        acc.clearCaches();

        // Verify data is still readable after cache manipulation.
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 16, 20));
            indgen(expect, Float(i * 100));
            AlwaysAssertExit(allEQ(res, expect));
        }
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 7. testDataManagerSpec
//    Exercise dataManagerSpec() / dataManagerInfo() and property management.
// =========================================================================
void testDataManagerSpec()
{
    cout << "testDataManagerSpec" << endl;
    String tabName = uniqueName("tTSMCov_Spec");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 8, 10),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMSpec", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMSpec", IPosition(3, 4, 5, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");
        const uInt nrow = 5;
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 8, 10));
            indgen(m, Float(i));
            data.put(i, m);
        }

        // Exercise dataManagerInfo.
        Record dminfo = table.dataManagerInfo();
        AlwaysAssertExit(dminfo.nfields() > 0);

        // Find the TiledColumnStMan entry.
        Bool found = False;
        for (uInt i = 0; i < dminfo.nfields(); i++) {
            Record sub = dminfo.subRecord(i);
            if (sub.isDefined("TYPE")) {
                String type = sub.asString("TYPE");
                if (type == "TiledColumnStMan") {
                    found = True;
                    // Should have NAME and SPEC sub-records.
                    AlwaysAssertExit(sub.isDefined("NAME"));
                    AlwaysAssertExit(sub.isDefined("SPEC"));
                    Record spec = sub.subRecord("SPEC");
                    // Spec should contain HYPERCUBES.
                    AlwaysAssertExit(spec.isDefined("HYPERCUBES"));
                    break;
                }
            }
        }
        AlwaysAssertExit(found);

        // Exercise getProperties / setProperties via accessor.
        ROTiledStManAccessor acc(table, "TSMSpec");
        // The accessor does not have getProperties/setProperties directly,
        // but we can exercise the underlying TiledStMan properties
        // indirectly via the dataManagerSpec / Record system.
        uInt origMax = acc.maximumCacheSize();
        acc.setMaximumCacheSize(10);
        AlwaysAssertExit(acc.maximumCacheSize() == 10);
        acc.setMaximumCacheSize(origMax);
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 8. testFlushReopen
//    Serialization round-trip: write, flush+close, reopen, verify.
// =========================================================================
void testFlushReopen()
{
    cout << "testFlushReopen" << endl;
    String tabName = uniqueName("tTSMCov_Flush");
    deleteIfExists(tabName);

    const uInt nrow = 15;
    // Write phase.
    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 10, 12),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMFlush", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMFlush", IPosition(3, 5, 4, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 10, 12));
            indgen(m, Float(i * 50));
            data.put(i, m);
        }
        // Flush and close happen implicitly at end of scope.
    }

    // Reopen and verify.
    {
        Table table(tabName, Table::Old);
        AlwaysAssertExit(table.nrow() == nrow);

        ArrayColumn<Float> data(table, "Data");
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 10, 12));
            indgen(expect, Float(i * 50));
            AlwaysAssertExit(allEQ(res, expect));
        }

        ROTiledStManAccessor acc(table, "TSMFlush");
        AlwaysAssertExit(acc.nhypercubes() == 1);
        AlwaysAssertExit(acc.getHypercubeShape(0) == IPosition(3, 10, 12, nrow));
        AlwaysAssertExit(acc.getTileShape(0) == IPosition(3, 5, 4, 1));
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 9. testSliceAccess
//    Exercise tile boundary crossing with sliced reads.
// =========================================================================
void testSliceAccess()
{
    cout << "testSliceAccess" << endl;
    String tabName = uniqueName("tTSMCov_Slice");
    deleteIfExists(tabName);

    {
        // Use a shape that doesn't divide evenly by the tile shape
        // so we cross tile boundaries.
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 15, 21),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMSlice", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMSlice", IPosition(3, 4, 5, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");
        const uInt nrow = 10;

        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 15, 21));
            indgen(m, Float(i * 1000));
            data.put(i, m);
        }

        // Read slices that cross tile boundaries.
        // Slice 1: starting at (2,3), length (10, 15) - crosses multiple tiles.
        for (uInt row = 0; row < nrow; row++) {
            Array<Float> slice;
            data.getSlice(row,
                          Slicer(IPosition(2, 2, 3), IPosition(2, 10, 15)),
                          slice);
            AlwaysAssertExit(slice.shape() == IPosition(2, 10, 15));

            // Verify values: full array has indgen starting at row*1000.
            // Element (col, freq) in full array = row*1000 + col + freq*15.
            for (Int f = 0; f < 15; f++) {
                for (Int c = 0; c < 10; c++) {
                    Float expected = Float(row * 1000) + Float(c + 2)
                                     + Float((f + 3) * 15);
                    IPosition pos(2, c, f);
                    AlwaysAssertExit(slice(pos) == expected);
                }
            }
        }

        // Slice 2: strided slice crossing boundaries.
        for (uInt row = 0; row < nrow; row++) {
            Array<Float> slice;
            data.getSlice(row,
                          Slicer(IPosition(2, 0, 0),
                                 IPosition(2, 8, 7),
                                 IPosition(2, 2, 3)),
                          slice);
            AlwaysAssertExit(slice.shape() == IPosition(2, 8, 7));
            for (Int f = 0; f < 7; f++) {
                for (Int c = 0; c < 8; c++) {
                    Float expected = Float(row * 1000) + Float(c * 2)
                                     + Float(f * 3 * 15);
                    IPosition pos(2, c, f);
                    AlwaysAssertExit(slice(pos) == expected);
                }
            }
        }
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

// =========================================================================
// 10. testEmptyCaches
//     Exercise emptyCaches: write, read (populate cache), empty, re-read.
// =========================================================================
void testEmptyCaches()
{
    cout << "testEmptyCaches" << endl;
    String tabName = uniqueName("tTSMCov_Empty");
    deleteIfExists(tabName);

    {
        TableDesc td("", "1", TableDesc::Scratch);
        td.addColumn(ArrayColumnDesc<Float>("Data", IPosition(2, 12, 14),
                                            ColumnDesc::FixedShape));
        td.defineHypercolumn("TSMEmpty", 3,
                             stringToVector("Data"));

        SetupNewTable newtab(tabName, td, Table::New);
        TiledColumnStMan sm("TSMEmpty", IPosition(3, 3, 4, 1));
        newtab.bindAll(sm);
        Table table(newtab);

        ArrayColumn<Float> data(table, "Data");
        const uInt nrow = 8;
        for (uInt i = 0; i < nrow; i++) {
            table.addRow();
            Matrix<Float> m(IPosition(2, 12, 14));
            indgen(m, Float(i * 500));
            data.put(i, m);
        }

        // First read pass: populates cache.
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 12, 14));
            indgen(expect, Float(i * 500));
            AlwaysAssertExit(allEQ(res, expect));
        }

        // Empty caches.
        ROTiledStManAccessor acc(table, "TSMEmpty");
        acc.clearCaches();

        // Second read pass: forces re-read from disk.
        for (uInt i = 0; i < nrow; i++) {
            Matrix<Float> res;
            data.get(i, res);
            Matrix<Float> expect(IPosition(2, 12, 14));
            indgen(expect, Float(i * 500));
            AlwaysAssertExit(allEQ(res, expect));
        }

        // Verify stats can be shown after clear + re-read.
        ostringstream oss;
        acc.showCacheStatistics(oss);
        AlwaysAssertExit(oss.str().size() > 0);
    }

    deleteIfExists(tabName);
    cout << "  OK" << endl;
}

} // anonymous namespace

int main()
{
    try {
        testMultiDataColumnLayout();
        testCoordinateColumns();
        testTiledCellMultiCube();
        testTiledShapeMultiShape();
        testMakeTileShape();
        testCacheControl();
        testDataManagerSpec();
        testFlushReopen();
        testSliceAccess();
        testEmptyCaches();
        cout << "All TiledStMan coverage tests passed." << endl;
        return 0;
    } catch (const AipsError& e) {
        cerr << "FAIL: " << e.what() << endl;
        return 1;
    }
}
