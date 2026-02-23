//# tStManColumnDispatch.cc: characterization coverage for StManColumn dispatch paths

#include <casacore/tables/DataMan/StManColumn.h>
#include <casacore/tables/Tables/RefRows.h>
#include <casacore/casa/Arrays/Array.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Arrays/Slicer.h>
#include <casacore/casa/Utilities/Assert.h>

#include <functional>
#include <iostream>

using namespace casacore;

namespace {

template<typename T>
T sampleValue();

template<>
Bool sampleValue<Bool>() { return True; }
template<>
uChar sampleValue<uChar>() { return uChar(3); }
template<>
Short sampleValue<Short>() { return Short(-4); }
template<>
uShort sampleValue<uShort>() { return uShort(5); }
template<>
Int sampleValue<Int>() { return Int(-6); }
template<>
uInt sampleValue<uInt>() { return uInt(7); }
template<>
Int64 sampleValue<Int64>() { return Int64(8); }
template<>
float sampleValue<float>() { return 1.25f; }
template<>
double sampleValue<double>() { return 2.5; }
template<>
Complex sampleValue<Complex>() { return Complex(3.0f, -1.0f); }
template<>
DComplex sampleValue<DComplex>() { return DComplex(4.0, 2.0); }
template<>
String sampleValue<String>() { return String("dispatch"); }

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

class DispatchColumn : public StManColumn {
public:
    explicit DispatchColumn(int dataType)
        : StManColumn(dataType), calls_p(0)
    {}

    uInt calls() const { return calls_p; }

#define OVERRIDE_ROW(T, NAME) \
    void get##NAME##V(uInt, T* dataPtr) override { *dataPtr = sampleValue<T>(); ++calls_p; } \
    void put##NAME##V(uInt, const T*) override { ++calls_p; }

#define OVERRIDE_BULK(T, NAME) \
    void getScalarColumn##NAME##V(Vector<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putScalarColumn##NAME##V(const Vector<T>*) override { ++calls_p; } \
    void getScalarColumnCells##NAME##V(const RefRows&, Vector<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putScalarColumnCells##NAME##V(const RefRows&, const Vector<T>*) override { ++calls_p; } \
    void getArray##NAME##V(uInt, Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putArray##NAME##V(uInt, const Array<T>*) override { ++calls_p; } \
    void getSlice##NAME##V(uInt, const Slicer&, Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putSlice##NAME##V(uInt, const Slicer&, const Array<T>*) override { ++calls_p; } \
    void getArrayColumn##NAME##V(Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putArrayColumn##NAME##V(const Array<T>*) override { ++calls_p; } \
    void getArrayColumnCells##NAME##V(const RefRows&, Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putArrayColumnCells##NAME##V(const RefRows&, const Array<T>*) override { ++calls_p; } \
    void getColumnSlice##NAME##V(const Slicer&, Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putColumnSlice##NAME##V(const Slicer&, const Array<T>*) override { ++calls_p; } \
    void getColumnSliceCells##NAME##V(const RefRows&, const Slicer&, Array<T>* dataPtr) override { dataPtr->set(sampleValue<T>()); ++calls_p; } \
    void putColumnSliceCells##NAME##V(const RefRows&, const Slicer&, const Array<T>*) override { ++calls_p; }

    OVERRIDE_ROW(Bool, Bool)
    OVERRIDE_ROW(uChar, uChar)
    OVERRIDE_ROW(Short, Short)
    OVERRIDE_ROW(uShort, uShort)
    OVERRIDE_ROW(Int, Int)
    OVERRIDE_ROW(uInt, uInt)
    OVERRIDE_ROW(float, float)
    OVERRIDE_ROW(double, double)
    OVERRIDE_ROW(Complex, Complex)
    OVERRIDE_ROW(DComplex, DComplex)
    OVERRIDE_ROW(String, String)

    OVERRIDE_BULK(Bool, Bool)
    OVERRIDE_BULK(uChar, uChar)
    OVERRIDE_BULK(Short, Short)
    OVERRIDE_BULK(uShort, uShort)
    OVERRIDE_BULK(Int, Int)
    OVERRIDE_BULK(uInt, uInt)
    OVERRIDE_BULK(Int64, Int64)
    OVERRIDE_BULK(float, float)
    OVERRIDE_BULK(double, double)
    OVERRIDE_BULK(Complex, Complex)
    OVERRIDE_BULK(DComplex, DComplex)
    OVERRIDE_BULK(String, String)

#undef OVERRIDE_ROW
#undef OVERRIDE_BULK

private:
    uInt calls_p;
};

template<typename T>
void exerciseDispatch(DispatchColumn& col, Bool hasOldScalarRowPath)
{
    T value = sampleValue<T>();
    Vector<T> vec(2);
    vec.set(value);
    Array<T> arr(IPosition(2, 2, 2));
    arr.set(value);

    RefRows rows(0, 1, 1);
    Slicer slicer(IPosition(2, 0, 0), IPosition(2, 1, 1));

    if (hasOldScalarRowPath) {
        col.get(0, &value);
        col.put(0, &value);
    }

    col.getScalarColumnV(vec);
    col.putScalarColumnV(vec);
    col.getScalarColumnCellsV(rows, vec);
    col.putScalarColumnCellsV(rows, vec);

    col.getArrayV(0, arr);
    col.putArrayV(0, arr);
    col.getSliceV(0, slicer, arr);
    col.putSliceV(0, slicer, arr);

    col.getArrayColumnV(arr);
    col.putArrayColumnV(arr);
    col.getArrayColumnCellsV(rows, arr);
    col.putArrayColumnCellsV(rows, arr);

    col.getColumnSliceV(slicer, arr);
    col.putColumnSliceV(slicer, arr);
    col.getColumnSliceCellsV(rows, slicer, arr);
    col.putColumnSliceCellsV(rows, slicer, arr);

    AlwaysAssertExit(col.calls() > 0);
}

void exerciseInvalidTypeDispatch()
{
    DispatchColumn other(TpOther);
    Vector<Int> vec(2, 0);
    Array<Int> arr(IPosition(2, 2, 2), 0);
    RefRows rows(0, 1, 1);
    Slicer slicer(IPosition(2, 0, 0), IPosition(2, 1, 1));

    expectThrows([&]() { other.getScalarColumnV(vec); });
    expectThrows([&]() { other.putScalarColumnV(vec); });
    expectThrows([&]() { other.getScalarColumnCellsV(rows, vec); });
    expectThrows([&]() { other.putScalarColumnCellsV(rows, vec); });
    expectThrows([&]() { other.getArrayV(0, arr); });
    expectThrows([&]() { other.putArrayV(0, arr); });
    expectThrows([&]() { other.getSliceV(0, slicer, arr); });
    expectThrows([&]() { other.putSliceV(0, slicer, arr); });
    expectThrows([&]() { other.getArrayColumnV(arr); });
    expectThrows([&]() { other.putArrayColumnV(arr); });
    expectThrows([&]() { other.getArrayColumnCellsV(rows, arr); });
    expectThrows([&]() { other.putArrayColumnCellsV(rows, arr); });
    expectThrows([&]() { other.getColumnSliceV(slicer, arr); });
    expectThrows([&]() { other.putColumnSliceV(slicer, arr); });
    expectThrows([&]() { other.getColumnSliceCellsV(rows, slicer, arr); });
    expectThrows([&]() { other.putColumnSliceCellsV(rows, slicer, arr); });
}

}  // namespace

int main()
{
    try {
        DispatchColumn meta(TpInt);
        expectThrows([&]() {
            meta.setShape(static_cast<uInt>(0), IPosition(1, 3));
        });
        expectThrows([&]() {
            meta.setShapeTiled(static_cast<uInt>(0), IPosition(1, 3),
                               IPosition(1, 1));
        });
        AlwaysAssertExit(meta.isShapeDefined(static_cast<uInt>(0)));
        AlwaysAssertExit(meta.ndim(static_cast<uInt>(0)) == 0);
        AlwaysAssertExit(meta.shape(static_cast<uInt>(0)).nelements() == 0);
        AlwaysAssertExit(meta.tileShape(static_cast<uInt>(0)).nelements() == 0);

        DispatchColumn cBool(TpBool);
        exerciseDispatch<Bool>(cBool, True);
        DispatchColumn cUChar(TpUChar);
        exerciseDispatch<uChar>(cUChar, True);
        DispatchColumn cShort(TpShort);
        exerciseDispatch<Short>(cShort, True);
        DispatchColumn cUShort(TpUShort);
        exerciseDispatch<uShort>(cUShort, True);
        DispatchColumn cInt(TpInt);
        exerciseDispatch<Int>(cInt, True);
        DispatchColumn cUInt(TpUInt);
        exerciseDispatch<uInt>(cUInt, True);
        DispatchColumn cInt64(TpInt64);
        exerciseDispatch<Int64>(cInt64, False);
        DispatchColumn cFloat(TpFloat);
        exerciseDispatch<float>(cFloat, True);
        DispatchColumn cDouble(TpDouble);
        exerciseDispatch<double>(cDouble, True);
        DispatchColumn cComplex(TpComplex);
        exerciseDispatch<Complex>(cComplex, True);
        DispatchColumn cDComplex(TpDComplex);
        exerciseDispatch<DComplex>(cDComplex, True);
        DispatchColumn cString(TpString);
        exerciseDispatch<String>(cString, True);

        exerciseInvalidTypeDispatch();
    } catch (const std::exception& x) {
        std::cerr << "tStManColumnDispatch failed: " << x.what() << std::endl;
        return 1;
    }
    return 0;
}
