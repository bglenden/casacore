//# tNullTable.cc: characterization coverage for NullTable null-object behavior

#include <casacore/tables/Tables/NullTable.h>
#include <casacore/tables/Tables/Table.h>
#include <casacore/tables/Tables/TableDesc.h>
#include <casacore/tables/Tables/TableError.h>
#include <casacore/tables/Tables/TableLock.h>
#include <casacore/tables/Tables/StorageOption.h>
#include <casacore/tables/Tables/ScaColDesc.h>
#include <casacore/tables/DataMan/StManAipsIO.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/Containers/Block.h>
#include <casacore/casa/Containers/Record.h>
#include <casacore/casa/Utilities/Assert.h>

#include <functional>
#include <memory>

using namespace casacore;

namespace {

void expectNullError(const String& marker, const std::function<void()>& fn)
{
    Bool thrown = False;
    try {
        fn();
    } catch (const TableError& err) {
        thrown = True;
        const String message(err.what());
        AlwaysAssertExit(message.contains("Table object is empty"));
        AlwaysAssertExit(message.contains(marker));
    } catch (const std::exception&) {
        thrown = True;
    }
    AlwaysAssertExit(thrown);
}

}  // namespace

int main()
{
    NullTable tab;
    AlwaysAssertExit(tab.isNull());

    TableLock lockOptions(TableLock::AutoLocking);
    StorageOption storage(StorageOption::SepFile);
    Record record;
    ScalarColumnDesc<Int> scalarDesc("A");
    TableDesc desc("", "", TableDesc::Scratch);
    desc.addColumn(ScalarColumnDesc<Int>("B"));
    StManAipsIO dataManager("dm");

    Vector<String> names(1);
    names(0) = "A";
    Vector<rownr_t> rownrs(1);
    rownrs(0) = 0;

    PtrBlock<BaseColumn*> sortCols(0);
    Block<std::shared_ptr<BaseCompare>> comparators(0);
    Block<Int> sortOrder(0);
    auto boundaries = std::make_shared<Vector<rownr_t>>();
    auto keyIdxChange = std::make_shared<Vector<size_t>>();

    expectNullError("reopenRW", [&]() { tab.reopenRW(); });
    expectNullError("asBigEndian", [&]() { (void)tab.asBigEndian(); });
    expectNullError("storageOption", [&]() { (void)tab.storageOption(); });
    expectNullError("isMultiUsed", [&]() { (void)tab.isMultiUsed(False); });
    expectNullError("lockOptions", [&]() { (void)tab.lockOptions(); });
    expectNullError("mergeLoc", [&]() { tab.mergeLock(lockOptions); });
    expectNullError("hasLock", [&]() { (void)tab.hasLock(FileLocker::Read); });
    expectNullError("lock", [&]() { (void)tab.lock(FileLocker::Write, 1); });
    expectNullError("unlock", [&]() { tab.unlock(); });
    expectNullError("flush", [&]() { tab.flush(False, False); });
    expectNullError("resync", [&]() { tab.resync(); });
    expectNullError("getModifyCounter", [&]() { (void)tab.getModifyCounter(); });
    expectNullError("isWritable", [&]() { (void)tab.isWritable(); });
    expectNullError("deepCopy", [&]() {
        tab.deepCopy("new.table", record, storage, Table::New, True, 0, False);
    });
    expectNullError("actualTableDesc", [&]() { (void)tab.actualTableDesc(); });
    expectNullError("dataManagerInfo", [&]() { (void)tab.dataManagerInfo(); });
    expectNullError("keywordSet", [&]() { (void)tab.keywordSet(); });
    expectNullError("rwKeywordSet", [&]() { (void)tab.rwKeywordSet(); });
    expectNullError("getColumn", [&]() { (void)tab.getColumn(0); });
    expectNullError("getColumn", [&]() { (void)tab.getColumn(String("A")); });
    expectNullError("canAddRow", [&]() { (void)tab.canAddRow(); });
    expectNullError("addRow", [&]() { tab.addRow(1, True); });
    expectNullError("canRemoveRow", [&]() { (void)tab.canRemoveRow(); });
    expectNullError("removeRow", [&]() { tab.removeRow(0); });
    expectNullError("findDataManager", [&]() {
        (void)tab.findDataManager(String("dm"), False);
    });
    expectNullError("addColumn", [&]() { tab.addColumn(scalarDesc, False); });
    expectNullError("addColumn", [&]() {
        tab.addColumn(scalarDesc, String("dm"), True, False);
    });
    expectNullError("addColumn", [&]() { tab.addColumn(scalarDesc, dataManager, False); });
    expectNullError("addColumn", [&]() { tab.addColumn(desc, dataManager, False); });
    expectNullError("canRemoveColumn", [&]() { (void)tab.canRemoveColumn(names); });
    expectNullError("removeColumn", [&]() { tab.removeColumn(names); });
    expectNullError("canRenameColumn", [&]() { (void)tab.canRenameColumn(String("A")); });
    expectNullError("renameColumn", [&]() { tab.renameColumn("B", "A"); });
    expectNullError("renameHypercolumn", [&]() { tab.renameHypercolumn("H2", "H1"); });
    expectNullError("rowNumbers", [&]() { (void)tab.rowNumbers(); });
    expectNullError("root", [&]() { (void)tab.root(); });
    expectNullError("rowOrder", [&]() { (void)tab.rowOrder(); });
    expectNullError("rowStorage", [&]() { (void)tab.rowStorage(); });
    expectNullError("adjustRownrs", [&]() {
        (void)tab.adjustRownrs(0, rownrs, False);
    });
    expectNullError("doSort", [&]() {
        (void)tab.doSort(sortCols, comparators, sortOrder, 0, boundaries, keyIdxChange);
    });
    expectNullError("renameSubTables", [&]() {
        tab.renameSubTables("new", "old");
    });

    return 0;
}
