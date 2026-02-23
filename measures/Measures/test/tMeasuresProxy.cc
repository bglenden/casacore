//# tMeasuresProxy.cc: characterization coverage for MeasuresProxy

#include <casacore/measures/Measures/MeasuresProxy.h>
#include <casacore/measures/Measures/MeasureHolder.h>
#include <casacore/measures/Measures/MDirection.h>
#include <casacore/measures/Measures/MEpoch.h>
#include <casacore/measures/Measures/MPosition.h>
#include <casacore/measures/Measures/MDoppler.h>
#include <casacore/measures/Measures/MRadialVelocity.h>
#include <casacore/measures/Measures/MFrequency.h>
#include <casacore/measures/Measures/MBaseline.h>
#include <casacore/measures/Measures/Muvw.h>
#include <casacore/measures/Measures/MEarthMagnetic.h>
#include <casacore/casa/Quanta/MVPosition.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Utilities/Assert.h>

#include <functional>
#include <iostream>

using namespace casacore;

namespace {

Record toRecord(const MeasureHolder& mh) {
    Record rec;
    String error;
    AlwaysAssertExit(mh.toRecord(error, rec));
    return rec;
}

void expectThrows(const std::function<void()>& fn, const char* label = "") {
    Bool thrown = False;
    try {
        fn();
    } catch (const std::exception&) {
        thrown = True;
    }
    if (!thrown) {
        std::cerr << "Expected throw did not occur: " << label << std::endl;
    }
    AlwaysAssertExit(thrown);
}

}  // namespace

int main() {
    try {
        MeasuresProxy proxy;

        Vector<String> observatories = proxy.obslist();
        AlwaysAssertExit(observatories.nelements() > 0);
        Record obs = proxy.observatory(observatories(0));
        AlwaysAssertExit(obs.nfields() > 0);

        Vector<String> lines = proxy.linelist();
        AlwaysAssertExit(lines.nelements() > 0);
        Record line = proxy.line(lines(0));
        AlwaysAssertExit(line.nfields() > 0);

        Vector<String> sources = proxy.srclist();
        if (sources.nelements() > 0) {
            Record src = proxy.source(sources(0));
            AlwaysAssertExit(src.nfields() > 0);
        }

        MDirection d1(Quantity(10, "deg"), Quantity(20, "deg"), MDirection::J2000);
        MDirection d2(Quantity(12, "deg"), Quantity(25, "deg"), MDirection::J2000);
        Record rd1 = toRecord(MeasureHolder(d1));
        Record rd2 = toRecord(MeasureHolder(d2));

        String shown = proxy.dirshow(rd1);
        AlwaysAssertExit(!shown.empty());
        Record allTypes = proxy.alltyp(rd1);
        AlwaysAssertExit(allTypes.isDefined("normal"));
        AlwaysAssertExit(proxy.posangle(rd1, rd2).getValue().nelements() == 1);
        AlwaysAssertExit(proxy.separation(rd1, rd2).getValue().nelements() == 1);

        MEpoch epoch(Quantity(55000.0, "d"), MEpoch::UTC);
        MPosition pos(MVPosition(0.0, 0.0, 6371000.0), MPosition::ITRF);
        AlwaysAssertExit(proxy.doframe(toRecord(MeasureHolder(epoch))));
        AlwaysAssertExit(proxy.doframe(toRecord(MeasureHolder(pos))));
        AlwaysAssertExit(proxy.doframe(rd1));

        Record dconv = proxy.measure(rd1, "B1950", Record());
        AlwaysAssertExit(dconv.nfields() > 0);

        MDoppler dop(Quantity(0.05), MDoppler::RADIO);
        Record rdop = toRecord(MeasureHolder(dop));
        Record rrv = proxy.doptorv(rdop, "LSRK");
        AlwaysAssertExit(rrv.nfields() > 0);

        Quantity restHz(1.420405751e9, "Hz");
        Record rfreq = proxy.doptofreq(rdop, "LSRK", restHz);
        AlwaysAssertExit(rfreq.nfields() > 0);
        Record rdopFromRV = proxy.todop(rrv, restHz);
        Record rdopFromFreq = proxy.todop(rfreq, restHz);
        AlwaysAssertExit(rdopFromRV.nfields() > 0);
        AlwaysAssertExit(rdopFromFreq.nfields() > 0);

        Record rrest = proxy.torest(rfreq, rdop);
        AlwaysAssertExit(rrest.nfields() > 0);

        // Exercise uvw/expand paths with a frame-attached baseline.
        MBaseline::Ref bref(MBaseline::ITRF);
        MBaseline base(MVBaseline(100.0, 20.0, 5.0), bref);
        Record rbase = toRecord(MeasureHolder(base));
        Record uvw = proxy.uvw(rbase);
        Record expanded = proxy.expand(uvw.asRecord("measure"));
        AlwaysAssertExit(uvw.isDefined("measure"));
        AlwaysAssertExit(uvw.isDefined("xyz"));
        AlwaysAssertExit(expanded.isDefined("measure"));
        AlwaysAssertExit(expanded.isDefined("xyz"));

        // Exercise additional measure conversion branches and offset handling.
        MFrequency mfreq(MVFrequency(1.420405751e9), MFrequency::TOPO);
        MRadialVelocity mrv(Quantity(1200.0, "m/s"), MRadialVelocity::LSRK);
        Muvw muvw(MVuvw(10.0, 20.0, 30.0), Muvw::J2000);
        MEarthMagnetic mem(MVEarthMagnetic(1e-6, 2e-6, 3e-6), MEarthMagnetic::ITRF);
        Record rfreq0 = toRecord(MeasureHolder(mfreq));
        Record rrv0 = toRecord(MeasureHolder(mrv));
        Record ruvw0 = toRecord(MeasureHolder(muvw));
        Record rem0 = toRecord(MeasureHolder(mem));

        AlwaysAssertExit(proxy.measure(toRecord(MeasureHolder(epoch)), "UTC", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(toRecord(MeasureHolder(pos)), "ITRF", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(rfreq0, "LSRK", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(rrv0, "LSRK", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(rbase, "J2000", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(ruvw0, "J2000", Record()).nfields() > 0);
        AlwaysAssertExit(proxy.measure(rem0, "ITRF", Record()).nfields() > 0);

        Record goodOffset = rd1;
        AlwaysAssertExit(proxy.measure(rd1, "J2000", goodOffset).nfields() > 0);
        Record badOffset;
        badOffset.define("x", Int(1));
        expectThrows([&proxy, &rd1, &badOffset]() {
            (void)proxy.measure(rd1, "J2000", badOffset);
        }, "measure bad offset");

        // Exercise vectorized baseline->uvw and uvw expansion branches.
        MeasureHolder mhBaseVec(base);
        mhBaseVec.makeMV(2);
        AlwaysAssertExit(mhBaseVec.setMV(0, MVBaseline(100.0, 20.0, 5.0)));
        AlwaysAssertExit(mhBaseVec.setMV(1, MVBaseline(110.0, 25.0, 6.0)));
        Record rbaseVec = toRecord(mhBaseVec);
        Record uvwVec = proxy.uvw(rbaseVec);
        AlwaysAssertExit(uvwVec.isDefined("dot"));
        AlwaysAssertExit(uvwVec.isDefined("xyz"));

        MeasureHolder mhUvwVec(muvw);
        mhUvwVec.makeMV(3);
        AlwaysAssertExit(mhUvwVec.setMV(0, MVuvw(1.0, 2.0, 3.0)));
        AlwaysAssertExit(mhUvwVec.setMV(1, MVuvw(2.0, 4.0, 6.0)));
        AlwaysAssertExit(mhUvwVec.setMV(2, MVuvw(3.0, 6.0, 9.0)));
        Record ruvwVec = toRecord(mhUvwVec);
        Record expandedVec = proxy.expand(ruvwVec);
        AlwaysAssertExit(expandedVec.isDefined("xyz"));

        // torest length mismatch path.
        MeasureHolder mhFreqVec(mfreq);
        mhFreqVec.makeMV(2);
        AlwaysAssertExit(mhFreqVec.setMV(0, MVFrequency(1.0e9)));
        AlwaysAssertExit(mhFreqVec.setMV(1, MVFrequency(1.1e9)));
        MeasureHolder mhDopVec(dop);
        mhDopVec.makeMV(1);
        AlwaysAssertExit(mhDopVec.setMV(0, MVDoppler(0.02)));
        expectThrows([&proxy, &mhFreqVec, &mhDopVec]() {
            (void)proxy.torest(toRecord(mhFreqVec), toRecord(mhDopVec));
        }, "torest mismatched lengths");

        // Negative-path checks.
        expectThrows([&proxy]() { (void)proxy.observatory("NO_SUCH_OBS"); }, "observatory missing");
        expectThrows([&proxy]() { (void)proxy.source("NO_SUCH_SOURCE"); }, "source missing");
        expectThrows([&proxy]() { (void)proxy.line("NO_SUCH_LINE"); }, "line missing");
        expectThrows([&proxy]() { (void)proxy.doframe(Record()); }, "doframe invalid record");
        // Characterization: unknown refs can resolve via DEFAULT instead of throwing.
        AlwaysAssertExit(proxy.measure(rdop, "BADREF", Record()).nfields() > 0);
        expectThrows([&proxy]() { (void)proxy.todop(Record(), Quantity(1.0, "Hz")); },
                     "todop wrong type");
        expectThrows([&proxy, &rbase]() { (void)proxy.expand(rbase); }, "expand baseline input");
        MeasuresProxy proxyNoFrame;
        expectThrows([&proxyNoFrame, &rbase]() { (void)proxyNoFrame.uvw(rbase); },
                     "uvw without frame");
        AlwaysAssertExit(!proxyNoFrame.doframe(rdop));
    } catch (const std::exception& x) {
        std::cerr << "tMeasuresProxy failed: " << x.what() << std::endl;
        return 1;
    }
    return 0;
}
