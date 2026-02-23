//# tTableProxy.cc: characterization coverage for TableProxy wrapper API

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

#include <fstream>
#include <functional>
#include <iostream>
#include <unistd.h>

using namespace casacore;

namespace {

String uniqueName(const String& base) {
    return base + "_" + String::toString(Int(getpid()));
}

void deleteIfExists(const String& name) {
    if (Table::isReadable(name)) {
        TableUtil::deleteTable(name, True);
    }
}

void expectThrows(const std::function<void()>& fn) {
    Bool thrown = False;
    try {
        fn();
    } catch (const std::exception&) {
        thrown = True;
    }
    AlwaysAssertExit(thrown);
}

void createInputTable(const String& tableName) {
    TableDesc td("", "", TableDesc::Scratch);
    td.addColumn(ScalarColumnDesc<Int>("I"));
    td.addColumn(ScalarColumnDesc<Double>("D"));
    td.addColumn(ScalarColumnDesc<String>("S"));
    td.addColumn(ArrayColumnDesc<Double>("AFIX", IPosition(2, 2, 2)));
    td.addColumn(ArrayColumnDesc<Int>("AVAR", 1));

    SetupNewTable newtab(tableName, td, Table::New);
    Table tab(newtab, 4);

    ScalarColumn<Int> iCol(tab, "I");
    ScalarColumn<Double> dCol(tab, "D");
    ScalarColumn<String> sCol(tab, "S");
    ArrayColumn<Double> aFix(tab, "AFIX");
    ArrayColumn<Int> aVar(tab, "AVAR");

    for (uInt r = 0; r < 4; ++r) {
        iCol.put(r, Int(r + 1));
        dCol.put(r, Double(0.5 + r));
        sCol.put(r, String("s") + String::toString(Int(r)));

        Matrix<Double> m(2, 2);
        m(0, 0) = r + 1;
        m(0, 1) = r + 2;
        m(1, 0) = r + 3;
        m(1, 1) = r + 4;
        aFix.put(r, m);

        Vector<Int> v(r + 1);
        for (uInt k = 0; k < v.nelements(); ++k) {
            v(k) = Int(10 * (r + 1) + k);
        }
        aVar.put(r, v);
    }
}

void checkBasicMetadata(TableProxy& p) {
    AlwaysAssertExit(p.isReadable());
    AlwaysAssertExit(p.isWritable());
    AlwaysAssertExit(p.nrows() == 4);
    AlwaysAssertExit(p.ncolumns() == 5);

    Vector<Int64> shp = p.shape();
    AlwaysAssertExit(shp.nelements() == 2);
    AlwaysAssertExit(shp(0) == 5);
    AlwaysAssertExit(shp(1) == 4);

    Vector<String> names = p.columnNames();
    AlwaysAssertExit(names.nelements() == 5);
    AlwaysAssertExit(anyEQ(names, String("I")));
    AlwaysAssertExit(anyEQ(names, String("AFIX")));

    AlwaysAssertExit(p.isScalarColumn("I"));
    AlwaysAssertExit(!p.isScalarColumn("AFIX"));
    AlwaysAssertExit(p.columnDataType("I") == "int");
    AlwaysAssertExit(p.columnDataType("S") == "string");
    AlwaysAssertExit(p.columnArrayType("AFIX").contains("fixed sized arrays"));
    AlwaysAssertExit(p.columnArrayType("AVAR").contains("variable sized arrays"));
}

void checkCellAndColumnIO(TableProxy& p) {
    AlwaysAssertExit(p.getCell("I", 2).asInt64() == 3);
    AlwaysAssertExit(p.getCell("S", 1).asString() == "s1");

    ValueHolder fix = p.getCell("AFIX", 0);
    Array<Double> fixArr = fix.asArrayDouble();
    AlwaysAssertExit(fixArr.shape().nelements() == 2);
    AlwaysAssertExit(fixArr.shape()(0) == 2);
    AlwaysAssertExit(fixArr.shape()(1) == 2);
    AlwaysAssertExit(fixArr(IPosition(2, 0, 0)) == 1.0);

    Vector<Int64> rows(2);
    rows(0) = 1;
    rows(1) = 3;
    p.putCell("I", rows, ValueHolder(Int(99)));
    p.putCell("S", rows, ValueHolder(String("patched")));
    AlwaysAssertExit(p.getCell("I", 1).asInt64() == 99);
    AlwaysAssertExit(p.getCell("I", 3).asInt64() == 99);
    AlwaysAssertExit(p.getCell("S", 1).asString() == "patched");

    Vector<Int> newVals(4);
    for (uInt i = 0; i < newVals.nelements(); ++i) {
        newVals(i) = Int(20 + i);
    }
    p.putColumn("I", 0, -1, 1, ValueHolder(newVals));
    Array<Int> got = p.getColumn("I", 0, -1, 1).asArrayInt();
    AlwaysAssertExit(got.nelements() == 4);
    AlwaysAssertExit(got(IPosition(1, 0)) == 20);
    AlwaysAssertExit(got(IPosition(1, 3)) == 23);

    Vector<Int64> row0(1);
    row0(0) = 0;
    Vector<Int> patched(2);
    patched(0) = 7;
    patched(1) = 8;
    p.putCell("AVAR", row0, ValueHolder(patched));
    Record out = p.getVarColumn("AVAR", 0, -1, 1);
    AlwaysAssertExit(out.nfields() == 4);
    Array<Int> check2 = out.asArrayInt("r2");
    AlwaysAssertExit(check2.nelements() == 2);
    AlwaysAssertExit(check2.data()[1] == 21);
    Array<Int> check0 = out.asArrayInt("r1");
    AlwaysAssertExit(check0.nelements() == 2);
    AlwaysAssertExit(check0.data()[0] == 7);

    Vector<Int> blc(2), trc(2), inc;
    blc(0) = 0; blc(1) = 0;
    trc(0) = 0; trc(1) = 1;
    ValueHolder slice = p.getCellSlice("AFIX", 0, blc, trc, inc);
    Array<Double> sv = slice.asArrayDouble();
    AlwaysAssertExit(sv.nelements() == 2);

    Matrix<Double> repl(1, 2);
    repl(0, 0) = 100.0;
    repl(0, 1) = 200.0;
    p.putCellSlice("AFIX", 0, blc, trc, inc, ValueHolder(repl));
    Array<Double> fixed0 = p.getCell("AFIX", 0).asArrayDouble();
    AlwaysAssertExit(fixed0(IPosition(2, 0, 0)) == 100.0);
    AlwaysAssertExit(fixed0(IPosition(2, 0, 1)) == 200.0);

    Vector<String> sfix = p.getColumnShapeString("AFIX", 0, -1, 1, False);
    Vector<String> svar = p.getColumnShapeString("AVAR", 0, -1, 1, False);
    AlwaysAssertExit(sfix.nelements() >= 1);
    AlwaysAssertExit(svar.nelements() == 4);
}

void checkKeywordsAndInfo(TableProxy& p) {
    p.putKeyword("", "TK", -1, False, ValueHolder(Int(42)));
    p.putKeyword("I", "CK", -1, False, ValueHolder(String("colkw")));
    AlwaysAssertExit(p.getKeyword("", "TK", -1).asInt64() == 42);
    AlwaysAssertExit(p.getKeyword("I", "CK", -1).asString() == "colkw");

    Record allKeys = p.getKeywordSet("");
    AlwaysAssertExit(allKeys.isDefined("TK"));
    Vector<String> fields = p.getFieldNames("", "", -1);
    AlwaysAssertExit(fields.nelements() >= 1);
    p.removeKeyword("I", "CK", -1);

    Record info = p.tableInfo();
    AlwaysAssertExit(info.isDefined("type"));
    Record newInfo;
    newInfo.define("type", String("coverage"));
    newInfo.define("subType", String("tableproxy"));
    newInfo.define("readme", String("wave1"));
    p.putTableInfo(newInfo);
    p.addReadmeLine("extra");
    Record info2 = p.tableInfo();
    AlwaysAssertExit(info2.asString("type") == "coverage");
}

void checkStaticAndErrorPaths(TableProxy& p) {
    Record lockRec;
    lockRec.define("option", String("usernoread"));
    lockRec.define("interval", 0.5);
    lockRec.define("maxwait", 2);
    TableLock tlock = TableProxy::makeLockOptions(lockRec);
    AlwaysAssertExit(tlock.option() == TableLock::UserLocking);

    AlwaysAssertExit(TableProxy::makeEndianFormat("little") == Table::LittleEndian);
    AlwaysAssertExit(TableProxy::makeEndianFormat("aipsrc") == Table::AipsrcEndian);
    expectThrows([]() { (void)TableProxy::makeEndianFormat("bad-endian"); });
    expectThrows([&p]() { (void)p.getCell("NO_SUCH_COL", 0); });
    expectThrows([&p]() {
        Record bad;
        bad.define("r1", Vector<Int>(1, 1));
        p.putVarColumn("AVAR", 0, -1, 1, bad);
    });
}

}  // namespace

int main() {
    const String inName = uniqueName("tTableProxy_cov_in.tab");
    const String copyName = uniqueName("tTableProxy_cov_copy.tab");
    const String renamedName = uniqueName("tTableProxy_cov_renamed.tab");
    const String selectedName = uniqueName("tTableProxy_cov_selected.tab");
    const String asciiName = uniqueName("tTableProxy_cov.txt");
    const String headerName = uniqueName("tTableProxy_cov.hdr");

    try {
        deleteIfExists(inName);
        deleteIfExists(copyName);
        deleteIfExists(renamedName);
        deleteIfExists(selectedName);

        createInputTable(inName);
        TableProxy p(inName, Record(), Table::Update);

        checkBasicMetadata(p);
        checkCellAndColumnIO(p);
        checkKeywordsAndInfo(p);
        checkStaticAndErrorPaths(p);

        Vector<String> outCols;
        Vector<Int> outPrec;
        (void)p.toAscii(asciiName, headerName, outCols, ",", outPrec, True);
        std::ifstream ascii(asciiName.c_str());
        std::ifstream hdr(headerName.c_str());
        AlwaysAssertExit(ascii.good());
        AlwaysAssertExit(hdr.good());

        TableProxy cpy = p.copy(copyName, False, True, False,
                                "little", Record(), False);
        AlwaysAssertExit(Table::isReadable(copyName));
        cpy.rename(renamedName);
        AlwaysAssertExit(cpy.tableName().contains(renamedName));
        cpy.close();
        TableProxy cpyRW(renamedName, Record(), Table::Update);
        AlwaysAssertExit(cpyRW.isWritable());
        cpyRW.addRow(1);
        p.copyRows(cpyRW, 0, -1, 1);
        cpyRW.close();

        Vector<Int64> selectRows(2);
        selectRows(0) = 0;
        selectRows(1) = 2;
        TableProxy sel = p.selectRows(selectRows, selectedName);
        AlwaysAssertExit(sel.nrows() == 2);

        p.flush(True);
        p.resync();
        p.reopenRW();
        p.close();
        sel.close();

        TableProxy delCopy(renamedName, Record(), Table::Update);
        delCopy.deleteTable(False);
        delCopy.close();
        TableProxy delSel(selectedName, Record(), Table::Update);
        delSel.deleteTable(False);
        delSel.close();
        TableProxy delIn(inName, Record(), Table::Update);
        delIn.deleteTable(False);
        delIn.close();
    } catch (const std::exception& x) {
        std::cerr << "tTableProxy failed: " << x.what() << std::endl;
        return 1;
    }

    return 0;
}
