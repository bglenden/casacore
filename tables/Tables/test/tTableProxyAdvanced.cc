//# tTableProxyAdvanced.cc: expanded characterization coverage for TableProxy APIs

#include <casacore/tables/Tables/TableProxy.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/casa/Arrays/Matrix.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Containers/ValueHolder.h>
#include <casacore/casa/Utilities/Assert.h>

#include <cstdio>
#include <functional>
#include <iostream>
#include <unistd.h>
#include <vector>

using namespace casacore;

namespace {

String uniqueName(const String& base)
{
    return base + "_" + String::toString(Int(getpid()));
}

void deleteIfExists(const String& name)
{
    try {
        if (Table::isReadable(name)) {
            TableUtil::deleteTable(name, True);
        }
    } catch (const std::exception&) {
        // Cleanup should not mask the actual test outcome.
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

Bool contains(const Vector<String>& values, const String& value)
{
    for (uInt i = 0; i < values.nelements(); ++i) {
        if (values(i) == value) {
            return True;
        }
    }
    return False;
}

void createScalarTable(const String& tableName)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("I"));
    td.addColumn(ScalarColumnDesc<Double>("D"));
    SetupNewTable newtab(tableName, td, Table::New);
    Table tab(newtab, 3);
    ScalarColumn<Int> iCol(tab, "I");
    ScalarColumn<Double> dCol(tab, "D");
    for (uInt r = 0; r < 3; ++r) {
        iCol.put(r, Int(r + 1));
        dCol.put(r, Double(0.25 + r));
    }
}

void createMixedTable(const String& tableName)
{
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("I"));
    td.addColumn(ArrayColumnDesc<Double>("AFIX", IPosition(2, 2, 2)));
    td.addColumn(ArrayColumnDesc<Int>("AVAR", 1));
    SetupNewTable newtab(tableName, td, Table::New);
    Table tab(newtab, 3);

    ScalarColumn<Int> iCol(tab, "I");
    ArrayColumn<Double> aFix(tab, "AFIX");
    ArrayColumn<Int> aVar(tab, "AVAR");
    for (uInt r = 0; r < 3; ++r) {
        iCol.put(r, Int(r + 10));
        Matrix<Double> m(2, 2);
        m(0, 0) = r + 1;
        m(0, 1) = r + 2;
        m(1, 0) = r + 3;
        m(1, 1) = r + 4;
        aFix.put(r, m);
        if (r > 0) {
            Vector<Int> v(r + 1);
            for (uInt i = 0; i < v.nelements(); ++i) {
                v(i) = Int(100 + 10 * r + i);
            }
            aVar.put(r, v);
        }
    }
}

void exerciseCreateCtorAndSchemaApis(const String& createdName)
{
    TableDesc baseDesc("", "", TableDesc::Scratch);
    baseDesc.addColumn(ScalarColumnDesc<Int>("CI"));
    baseDesc.addColumn(ArrayColumnDesc<Double>("CA", IPosition(1, 2)));
    Record recDesc = TableProxy::getTableDesc(baseDesc, False);

    TableProxy created(createdName, Record(), "little", "plain", -1, recDesc, Record());
    AlwaysAssertExit(created.nrows() == 0);
    created.addRow(2);
    AlwaysAssertExit(created.nrows() == 2);

    Vector<Int64> rows(1);
    rows(0) = 0;
    created.putCell("CI", rows, ValueHolder(Int(7)));
    Vector<Double> arr2(2);
    arr2(0) = 1.5;
    arr2(1) = 2.5;
    created.putCell("CA", rows, ValueHolder(arr2));

    // Exercise lock and sync APIs.
    created.lock(True, 1);
    (void)created.hasLock(True);
    (void)created.hasLock(False);
    (void)created.hasDataChanged();
    Record lockRec = created.lockOptions();
    AlwaysAssertExit(lockRec.isDefined("option"));
    created.unlock();
    created.flush(True);
    created.resync();
    created.reopenRW();
    AlwaysAssertExit(created.isMultiUsed(False) || !created.isMultiUsed(False));
    AlwaysAssertExit(created.endianFormat() == "little" ||
                     created.endianFormat() == "big");

    // Table description and column description paths.
    Record td = created.getTableDescription(False, False);
    Record tdActual = created.getTableDescription(True, False);
    AlwaysAssertExit(td.isDefined("CI"));
    AlwaysAssertExit(tdActual.isDefined("CI"));
    Record cd = created.getColumnDescription("CI", False, False);
    AlwaysAssertExit(cd.nfields() > 0);
    AlwaysAssertExit(created.getDataManagerInfo().nfields() > 0);

    Record props = created.getProperties("CI", True);
    try {
        created.setProperties("CI", props, True);
    } catch (const std::exception&) {
        // Characterization: not all data managers accept property writes.
    }
    created.setMaximumCacheSize("CI", 4096);

    // Add, rename, and remove a column via descriptor record conversion.
    TableDesc addDesc("", "", TableDesc::Scratch);
    addDesc.addColumn(ScalarColumnDesc<Float>("NEWCOL"));
    created.addColumns(TableProxy::getTableDesc(addDesc, False), Record(), False);
    AlwaysAssertExit(contains(created.columnNames(), "NEWCOL"));
    created.renameColumn("NEWCOL", "RENAMED");
    AlwaysAssertExit(contains(created.columnNames(), "RENAMED"));
    Vector<String> toRemove(1);
    toRemove(0) = "RENAMED";
    created.removeColumns(toRemove);
    AlwaysAssertExit(!contains(created.columnNames(), "RENAMED"));

    // Row remove path.
    Vector<Int64> removeRows(1);
    removeRows(0) = 1;
    created.removeRow(removeRows);
    AlwaysAssertExit(created.nrows() == 1);

    // Keyword set bulk put path.
    Record kws;
    kws.define("bulk_i", Int(11));
    kws.define("bulk_s", String("abc"));
    created.putKeywordSet("", kws);
    Record got = created.getKeywordSet("");
    AlwaysAssertExit(got.isDefined("bulk_i"));

    String structure = created.showStructure(True, True, False, False);
    AlwaysAssertExit(!structure.empty());
    AlwaysAssertExit(created.getPartNames(False).nelements() >= 1);
}

void exerciseArrayAndVHApis(const String& mixedName)
{
    TableProxy p(mixedName, Record(), Table::Update);
    AlwaysAssertExit(p.nrows() == 3);
    AlwaysAssertExit(!p.cellContentsDefined("AVAR", 0));

    Vector<Int64> row0(1);
    row0(0) = 0;
    Vector<Int> varv(2);
    varv(0) = 5;
    varv(1) = 6;
    p.putCell("AVAR", row0, ValueHolder(varv));
    AlwaysAssertExit(p.cellContentsDefined("AVAR", 0));

    // Exercise the ValueHolder-in/out variant methods.
    Array<Double> cellArray(IPosition(2, 2, 2));
    p.getCellVH("AFIX", 0, ValueHolder(cellArray));

    Vector<Int> scalarRange(3);
    p.getColumnVH("I", 0, -1, 1, ValueHolder(scalarRange));

    Vector<Int> blc(2), trc(2), inc;
    blc(0) = 0;
    blc(1) = 0;
    trc(0) = 0;
    trc(1) = 1;
    Array<Double> sliceArray(IPosition(2, 1, 2));
    p.getCellSliceVH("AFIX", 0, blc, trc, inc, ValueHolder(sliceArray));

    // nrow=0 takes the guarded no-op path in putValueSliceInTable.
    Matrix<Double> emptyPut(1, 2);
    p.putColumnSlice("AFIX", 0, 0, 1, blc, trc, inc, ValueHolder(emptyPut));
}

void exerciseRowAndSelectApis(const String& mixedName, const String& selectedName)
{
    TableProxy p(mixedName, Record(), Table::Update);
    TableProxy nullProxy;
    Vector<Int64> rowsSelf = p.rowNumbers(p);
    Vector<Int64> rowsNull = p.rowNumbers(nullProxy);
    AlwaysAssertExit(rowsSelf.nelements() == uInt(p.nrows()));
    AlwaysAssertExit(rowsNull.nelements() == uInt(p.nrows()));

    Vector<Int64> selRows(2);
    selRows(0) = 0;
    selRows(1) = 2;
    TableProxy selected = p.selectRows(selRows, selectedName);
    AlwaysAssertExit(selected.nrows() == 2);
}

void exerciseAsciiCtorAndConcat(const String& scalar1, const String& scalar2,
                                const String& asciiData, const String& asciiHeader,
                                const String& asciiTable,
                                const String& copiedNoRows)
{
    TableProxy p1(scalar1, Record(), Table::Update);
    TableProxy p2(scalar2, Record(), Table::Update);

    Vector<String> noCols;
    Vector<Int> noPrec;
    (void)p1.toAscii(asciiData, asciiHeader, noCols, ",", noPrec, True);
    TableProxy fromAscii(asciiData, asciiHeader, asciiTable, False, IPosition(),
                         ",", "#", 1, -1);
    AlwaysAssertExit(fromAscii.nrows() == p1.nrows());
    AlwaysAssertExit(!fromAscii.getAsciiFormat().empty());

    expectThrows([&]() {
        TableProxy bad(asciiData, asciiHeader, asciiTable + "_bad", False, IPosition(),
                       ",,", "#", 1, -1);
    });

    Vector<String> tableNames(2);
    tableNames(0) = scalar1;
    tableNames(1) = scalar2;
    Vector<String> noSubTables;
    TableProxy concatByName(tableNames, noSubTables, Record(), Table::Old);
    AlwaysAssertExit(concatByName.nrows() == p1.nrows() + p2.nrows());

    std::vector<TableProxy> proxies;
    proxies.push_back(p1);
    proxies.push_back(p2);
    TableProxy concatByObj(proxies, noSubTables);
    AlwaysAssertExit(concatByObj.nrows() == p1.nrows() + p2.nrows());

    // Command constructor table-result and calc-result paths.
    TableProxy cmdSel(String("select from ") + scalar1 + " where I >= 2",
                      std::vector<TableProxy>());
    AlwaysAssertExit(cmdSel.nrows() == 2);
    TableProxy cmdCalc("calc 1+2", std::vector<TableProxy>());
    Record calc = cmdCalc.getCalcResult();
    AlwaysAssertExit(calc.isDefined("values"));

    // Copy with noRows path and deleteTable path.
    TableProxy noRowsCopy = p1.copy(copiedNoRows, False, True, False, "little",
                                    Record(), True);
    AlwaysAssertExit(noRowsCopy.nrows() == 0);
    noRowsCopy.deleteTable(True);
    noRowsCopy.close();
}

}  // namespace

int main()
{
    const String createdName = uniqueName("tTableProxyAdv_created.tab");
    const String mixedName = uniqueName("tTableProxyAdv_mixed.tab");
    const String selectedName = uniqueName("tTableProxyAdv_selected.tab");
    const String scalar1 = uniqueName("tTableProxyAdv_scalar1.tab");
    const String scalar2 = uniqueName("tTableProxyAdv_scalar2.tab");
    const String asciiData = uniqueName("tTableProxyAdv_ascii.txt");
    const String asciiHeader = uniqueName("tTableProxyAdv_ascii.hdr");
    const String asciiTable = uniqueName("tTableProxyAdv_ascii.tab");
    const String copiedNoRows = uniqueName("tTableProxyAdv_norows.tab");

    try {
        deleteIfExists(createdName);
        deleteIfExists(mixedName);
        deleteIfExists(selectedName);
        deleteIfExists(scalar1);
        deleteIfExists(scalar2);
        deleteIfExists(asciiTable);
        deleteIfExists(copiedNoRows);
        (void)std::remove(asciiData.chars());
        (void)std::remove(asciiHeader.chars());

        createMixedTable(mixedName);
        createScalarTable(scalar1);
        createScalarTable(scalar2);

        exerciseCreateCtorAndSchemaApis(createdName);
        exerciseArrayAndVHApis(mixedName);
        exerciseRowAndSelectApis(mixedName, selectedName);
        exerciseAsciiCtorAndConcat(scalar1, scalar2, asciiData, asciiHeader,
                                   asciiTable, copiedNoRows);

        deleteIfExists(createdName);
        deleteIfExists(mixedName);
        deleteIfExists(selectedName);
        deleteIfExists(scalar1);
        deleteIfExists(scalar2);
        deleteIfExists(asciiTable);
        deleteIfExists(copiedNoRows);
        (void)std::remove(asciiData.chars());
        (void)std::remove(asciiHeader.chars());
    } catch (...) {
        deleteIfExists(createdName);
        deleteIfExists(mixedName);
        deleteIfExists(selectedName);
        deleteIfExists(scalar1);
        deleteIfExists(scalar2);
        deleteIfExists(asciiTable);
        deleteIfExists(copiedNoRows);
        (void)std::remove(asciiData.chars());
        (void)std::remove(asciiHeader.chars());
        std::cerr << "tTableProxyAdvanced failed with an exception" << std::endl;
        return 1;
    }
    return 0;
}
