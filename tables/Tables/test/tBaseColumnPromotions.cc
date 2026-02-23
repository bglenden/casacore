//# tBaseColumnPromotions.cc: exercise BaseColumn type promotion and scalar/array error paths

#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/SetupNewTab.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/Tables/ArrColDesc.h>
#include <casacore/tables/Tables/ScalarColumn.h>
#include <casacore/tables/Tables/ArrayColumn.h>
#include <casacore/tables/Tables/TableColumn.h>
#include <casacore/tables/Tables/TableUtil.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/BasicSL/Complex.h>
#include <casacore/casa/Utilities/Assert.h>

#include <cmath>
#include <functional>
#include <iostream>
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

Bool nearFloat(float lhs, float rhs)
{
    return std::abs(lhs - rhs) < 1e-5f;
}

Bool nearDouble(double lhs, double rhs)
{
    return std::abs(lhs - rhs) < 1e-10;
}

}  // namespace

int main()
{
    const String tableName = uniqueName("tBaseColumnPromotions.tab");
    try {
        deleteIfExists(tableName);

        TableDesc td("", "", TableDesc::Scratch);
        td.addColumn(ScalarColumnDesc<Bool>("B"));
        td.addColumn(ScalarColumnDesc<uChar>("UC"));
        td.addColumn(ScalarColumnDesc<Short>("S"));
        td.addColumn(ScalarColumnDesc<uShort>("US"));
        td.addColumn(ScalarColumnDesc<Int>("I"));
        td.addColumn(ScalarColumnDesc<uInt>("UI"));
        td.addColumn(ScalarColumnDesc<Int64>("I64"));
        td.addColumn(ScalarColumnDesc<Float>("F"));
        td.addColumn(ScalarColumnDesc<Double>("D"));
        td.addColumn(ScalarColumnDesc<Complex>("C"));
        td.addColumn(ScalarColumnDesc<DComplex>("DC"));
        td.addColumn(ScalarColumnDesc<String>("STR"));
        td.addColumn(ArrayColumnDesc<Int>("ARRI", 1));

        SetupNewTable newtab(tableName, td, Table::New);
        Table tab(newtab, 1);

        ScalarColumn<Bool>(tab, "B").put(0, True);
        ScalarColumn<uChar>(tab, "UC").put(0, uChar(7));
        ScalarColumn<Short>(tab, "S").put(0, Short(-5));
        ScalarColumn<uShort>(tab, "US").put(0, uShort(9));
        ScalarColumn<Int>(tab, "I").put(0, Int(-11));
        ScalarColumn<uInt>(tab, "UI").put(0, uInt(13));
        ScalarColumn<Int64>(tab, "I64").put(0, Int64(17));
        ScalarColumn<Float>(tab, "F").put(0, Float(1.25f));
        ScalarColumn<Double>(tab, "D").put(0, Double(2.5));
        ScalarColumn<Complex>(tab, "C").put(0, Complex(3.0f, -1.0f));
        ScalarColumn<DComplex>(tab, "DC").put(0, DComplex(4.0, 2.0));
        ScalarColumn<String>(tab, "STR").put(0, String("abc"));
        Vector<Int> arrv(2);
        arrv(0) = 10;
        arrv(1) = 20;
        ArrayColumn<Int>(tab, "ARRI").put(0, arrv);

        TableColumn cB(tab, "B");
        TableColumn cUC(tab, "UC");
        TableColumn cS(tab, "S");
        TableColumn cUS(tab, "US");
        TableColumn cI(tab, "I");
        TableColumn cUI(tab, "UI");
        TableColumn cI64(tab, "I64");
        TableColumn cF(tab, "F");
        TableColumn cD(tab, "D");
        TableColumn cC(tab, "C");
        TableColumn cDC(tab, "DC");
        TableColumn cStr(tab, "STR");
        TableColumn cArr(tab, "ARRI");

        // getScalar coverage including many promotion paths.
        Bool vb = False;
        cB.getScalar(0, vb);
        AlwaysAssertExit(vb);

        uChar vuc = 0;
        cUC.getScalar(0, vuc);
        AlwaysAssertExit(vuc == 7);

        Short vs = 0;
        cS.getScalar(0, vs);
        AlwaysAssertExit(vs == -5);

        uShort vus = 0;
        cUC.getScalar(0, vus);
        AlwaysAssertExit(vus == 7);
        cUS.getScalar(0, vus);
        AlwaysAssertExit(vus == 9);

        Int vi = 0;
        cUC.getScalar(0, vi);
        AlwaysAssertExit(vi == 7);
        cS.getScalar(0, vi);
        AlwaysAssertExit(vi == -5);
        cUS.getScalar(0, vi);
        AlwaysAssertExit(vi == 9);
        cI.getScalar(0, vi);
        AlwaysAssertExit(vi == -11);

        uInt vui = 0;
        cUC.getScalar(0, vui);
        AlwaysAssertExit(vui == 7);
        cUS.getScalar(0, vui);
        AlwaysAssertExit(vui == 9);
        cUI.getScalar(0, vui);
        AlwaysAssertExit(vui == 13);

        Int64 vi64 = 0;
        cUC.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == 7);
        cS.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == -5);
        cUS.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == 9);
        cI.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == -11);
        cUI.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == 13);
        cI64.getScalar(0, vi64);
        AlwaysAssertExit(vi64 == 17);

        float vf = 0;
        cUC.getScalar(0, vf);
        cS.getScalar(0, vf);
        cUS.getScalar(0, vf);
        cI.getScalar(0, vf);
        cUI.getScalar(0, vf);
        cI64.getScalar(0, vf);
        cF.getScalar(0, vf);
        AlwaysAssertExit(nearFloat(vf, 1.25f));
        cD.getScalar(0, vf);
        AlwaysAssertExit(nearFloat(vf, 2.5f));

        double vd = 0;
        cUC.getScalar(0, vd);
        cS.getScalar(0, vd);
        cUS.getScalar(0, vd);
        cI.getScalar(0, vd);
        cUI.getScalar(0, vd);
        cI64.getScalar(0, vd);
        cF.getScalar(0, vd);
        cD.getScalar(0, vd);
        AlwaysAssertExit(nearDouble(vd, 2.5));

        Complex vc;
        cUC.getScalar(0, vc);
        cS.getScalar(0, vc);
        cUS.getScalar(0, vc);
        cI.getScalar(0, vc);
        cUI.getScalar(0, vc);
        cI64.getScalar(0, vc);
        cF.getScalar(0, vc);
        cD.getScalar(0, vc);
        cC.getScalar(0, vc);
        cDC.getScalar(0, vc);
        AlwaysAssertExit(nearFloat(vc.real(), 4.0f));

        DComplex vdc;
        cUC.getScalar(0, vdc);
        cS.getScalar(0, vdc);
        cUS.getScalar(0, vdc);
        cI.getScalar(0, vdc);
        cUI.getScalar(0, vdc);
        cI64.getScalar(0, vdc);
        cF.getScalar(0, vdc);
        cD.getScalar(0, vdc);
        cC.getScalar(0, vdc);
        cDC.getScalar(0, vdc);
        AlwaysAssertExit(nearDouble(vdc.real(), 4.0));

        String vstr;
        cStr.getScalar(0, vstr);
        AlwaysAssertExit(vstr == "abc");

        // putScalar coverage across many promotion paths.
        cB.putScalar(0, False);

        cUC.putScalar(0, uChar(1));
        cS.putScalar(0, uChar(2));
        cUS.putScalar(0, uChar(3));
        cI.putScalar(0, uChar(4));
        cUI.putScalar(0, uChar(5));
        cI64.putScalar(0, uChar(6));
        cF.putScalar(0, uChar(7));
        cD.putScalar(0, uChar(8));
        cC.putScalar(0, uChar(9));
        cDC.putScalar(0, uChar(10));

        cS.putScalar(0, Short(-2));
        cI.putScalar(0, Short(-3));
        cI64.putScalar(0, Short(-4));
        cF.putScalar(0, Short(11));
        cD.putScalar(0, Short(12));
        cC.putScalar(0, Short(13));
        cDC.putScalar(0, Short(14));

        cUS.putScalar(0, uShort(15));
        cI.putScalar(0, uShort(16));
        cUI.putScalar(0, uShort(17));
        cI64.putScalar(0, uShort(18));
        cF.putScalar(0, uShort(19));
        cD.putScalar(0, uShort(20));
        cC.putScalar(0, uShort(21));
        cDC.putScalar(0, uShort(22));

        cI.putScalar(0, Int(23));
        cI64.putScalar(0, Int(24));
        cF.putScalar(0, Int(25));
        cD.putScalar(0, Int(26));
        cC.putScalar(0, Int(27));
        cDC.putScalar(0, Int(28));

        cUI.putScalar(0, uInt(29));
        cI64.putScalar(0, uInt(30));
        cF.putScalar(0, uInt(31));
        cD.putScalar(0, uInt(32));
        cC.putScalar(0, uInt(33));
        cDC.putScalar(0, uInt(34));

        cI64.putScalar(0, Int64(35));
        cF.putScalar(0, Int64(36));
        cD.putScalar(0, Int64(37));
        cC.putScalar(0, Int64(38));
        cDC.putScalar(0, Int64(39));

        cF.putScalar(0, float(40.5f));
        cD.putScalar(0, float(41.5f));
        cC.putScalar(0, float(42.5f));
        cDC.putScalar(0, float(43.5f));

        cF.putScalar(0, double(44.25));
        cD.putScalar(0, double(45.25));
        cC.putScalar(0, double(46.25));
        cDC.putScalar(0, double(47.25));

        cC.putScalar(0, Complex(48.0f, 1.0f));
        cDC.putScalar(0, Complex(49.0f, 2.0f));

        cC.putScalar(0, DComplex(50.0, 3.0));
        cDC.putScalar(0, DComplex(51.0, 4.0));

        cStr.putScalar(0, String("xyz"));

        // Sanity checks after puts.
        cB.getScalar(0, vb);
        AlwaysAssertExit(!vb);
        cD.getScalar(0, vd);
        AlwaysAssertExit(nearDouble(vd, 45.25));
        cStr.getScalar(0, vstr);
        AlwaysAssertExit(vstr == "xyz");

        // Error-path coverage.
        expectThrows([&]() {
            Bool tmp = False;
            cUC.getScalar(0, tmp);
        });
        expectThrows([&]() { cB.putScalar(0, Int(7)); });
        expectThrows([&]() {
            Int tmp = 0;
            cArr.getScalar(0, tmp);
        });
        expectThrows([&]() { cArr.putScalar(0, Int(7)); });

        expectThrows([&]() { (void)cI.ndimColumn(); });
        expectThrows([&]() { (void)cI.shapeColumn(); });
        expectThrows([&]() { (void)cI.ndim(0); });
        expectThrows([&]() { (void)cI.shape(0); });
        expectThrows([&]() { (void)cI.tileShape(0); });

        AlwaysAssertExit(cArr.ndim(0) == 1);

        tab.flush(True);
        tab.unlock();
        tab = Table();

        deleteIfExists(tableName);
    } catch (const std::exception& x) {
        deleteIfExists(tableName);
        std::cerr << "tBaseColumnPromotions failed: " << x.what() << std::endl;
        return 1;
    }
    return 0;
}
