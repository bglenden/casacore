//# tDOosCoverage.cc: characterization coverage for legacy DOos file APIs

#include <casacore/casa/OS/DOos.h>
#include <casacore/casa/OS/File.h>
#include <casacore/casa/OS/Path.h>
#include <casacore/casa/OS/RegularFile.h>
#include <casacore/casa/OS/Directory.h>
#include <casacore/casa/OS/SymLink.h>
#include <casacore/casa/Arrays/Vector.h>
#include <casacore/casa/BasicSL/String.h>
#include <casacore/casa/Utilities/Assert.h>

#include <fstream>
#include <functional>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

using namespace casacore;

namespace {

String uniqueName(const String& base)
{
    return base + "_" + String::toString(Int(getpid()));
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

Bool contains(const Vector<String>& values, const String& needle)
{
    for (uInt i = 0; i < values.nelements(); ++i) {
        if (values(i) == needle) {
            return True;
        }
    }
    return False;
}

void writeBytes(const String& path, const std::string& bytes)
{
    std::ofstream out(path.chars(), std::ios::binary);
    out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
    AlwaysAssertExit(out.good());
}

void removeTreeIfExists(const String& path)
{
    File file(path);
    if (!file.exists()) {
        return;
    }
    if (file.isDirectory(False) && !file.isSymLink()) {
        Directory(file).removeRecursive();
    } else if (file.isSymLink()) {
        SymLink(file).remove();
    } else if (file.isRegular(False)) {
        RegularFile(file).remove();
    } else {
        (void)::unlink(path.chars());
    }
}

}  // namespace

int main()
{
    const String root = uniqueName("tDOosCoverage_tmp");
    const String regular = root + "/regular.txt";
    const String executable = root + "/exec.sh";
    const String hidden = root + "/.hidden";
    const String subdir = root + "/sub";
    const String subfile = subdir + "/payload.bin";
    const String tableDir = root + "/tableLike";
    const String tableDat = tableDir + "/table.dat";
    const String linkReg = root + "/link_regular";
    const String linkDir = root + "/link_subdir";
    const String fifoName = root + "/fifo.node";
    const String missing = root + "/does_not_exist";

    try {
        removeTreeIfExists(root);
        Directory(root).create();
        Directory(subdir).create();
        Directory(tableDir).create();

        writeBytes(regular, "abcde");
        writeBytes(executable, "#!/bin/sh\necho x\n");
        writeBytes(hidden, "h");
        writeBytes(subfile, "pq");
        writeBytes(tableDat, "t");

        File(executable).setPermissions(0755);
        // Keep symlink targets directory-relative for fixture portability.
        SymLink(linkReg).create("regular.txt");
        SymLink(linkDir).create("sub");

        AlwaysAssertExit(::mkfifo(fifoName.chars(), 0600) == 0);

        {
            Vector<String> names(3);
            names(0) = "";
            names(1) = regular;
            names(2) = root + "/new.file";
            Vector<Bool> valid = DOos::isValidPathName(names);
            AlwaysAssertExit(valid.nelements() == 3);
            AlwaysAssertExit(!valid(0));
            AlwaysAssertExit(valid(1));
            AlwaysAssertExit(valid(2));
        }

        {
            Vector<String> names(4);
            names(0) = "";
            names(1) = regular;
            names(2) = linkReg;
            names(3) = missing;
            Vector<Bool> existsNoFollow = DOos::fileExists(names, False);
            Vector<Bool> existsFollow = DOos::fileExists(names, True);
            AlwaysAssertExit(!existsNoFollow(0));
            AlwaysAssertExit(existsNoFollow(1));
            AlwaysAssertExit(existsNoFollow(2));
            AlwaysAssertExit(!existsNoFollow(3));
            AlwaysAssertExit(!existsFollow(0));
            AlwaysAssertExit(existsFollow(1));
            AlwaysAssertExit(existsFollow(2));
            AlwaysAssertExit(!existsFollow(3));
        }

        {
            Vector<String> names(6);
            names(0) = regular;
            names(1) = root;
            names(2) = tableDir;
            names(3) = linkReg;
            names(4) = missing;
            names(5) = fifoName;
            Vector<String> typesNoFollow = DOos::fileType(names, False);
            Vector<String> typesFollow = DOos::fileType(names, True);
            AlwaysAssertExit(typesNoFollow(0) == "Regular File");
            AlwaysAssertExit(typesNoFollow(1) == "Directory");
            AlwaysAssertExit(typesNoFollow(2) == "Table");
            AlwaysAssertExit(typesNoFollow(3) == "SymLink");
            AlwaysAssertExit(typesNoFollow(4) == "Invalid");
            AlwaysAssertExit(typesNoFollow(5) == "Unknown");
            AlwaysAssertExit(typesFollow(3) == "Regular File");
        }

        {
            Vector<String> visible = DOos::fileNames(root, "", "", False, True);
            Vector<String> withHidden = DOos::fileNames(root, "", "", True, True);
            AlwaysAssertExit(!contains(visible, ".hidden"));
            AlwaysAssertExit(contains(withHidden, ".hidden"));

            Vector<String> patternTxt = DOos::fileNames(root, "*.txt", "r", False, True);
            AlwaysAssertExit(contains(patternTxt, "regular.txt"));

            Vector<String> onlyDirs = DOos::fileNames(root, "", "d", False, True);
            AlwaysAssertExit(contains(onlyDirs, "sub"));
            AlwaysAssertExit(contains(onlyDirs, "tableLike"));

            Vector<String> onlySymLinks = DOos::fileNames(root, "", "s", False, False);
            AlwaysAssertExit(contains(onlySymLinks, "link_regular"));
            AlwaysAssertExit(contains(onlySymLinks, "link_subdir"));

            Vector<String> executableFiles = DOos::fileNames(root, "", "rX", False, True);
            AlwaysAssertExit(contains(executableFiles, "exec.sh"));
        }

        {
            Vector<String> makeNames(1);
            makeNames(0) = root + "/nested/a/b";
            DOos::makeDirectory(makeNames, True);
            AlwaysAssertExit(File(makeNames(0)).isDirectory());

            Vector<String> bad(1);
            bad(0) = regular;
            expectThrows([&]() { DOos::makeDirectory(bad, False); });
        }

        {
            Vector<String> names(2);
            names(0) = regular;
            names(1) = subfile;
            Vector<String> full = DOos::fullName(names);
            Vector<String> dirs = DOos::dirName(names);
            Vector<String> base = DOos::baseName(names);
            AlwaysAssertExit(full(0) == Path(regular).absoluteName());
            AlwaysAssertExit(full(1) == Path(subfile).absoluteName());
            AlwaysAssertExit(base(0) == "regular.txt");
            AlwaysAssertExit(base(1) == "payload.bin");
            AlwaysAssertExit(dirs(0) == Path(Path(regular).absoluteName()).dirName());
            AlwaysAssertExit(dirs(1).contains("/sub"));
        }

        {
            Vector<String> names(1);
            names(0) = regular;
            Vector<Double> t1 = DOos::fileTime(names, 1, True);
            Vector<Double> t2 = DOos::fileTime(names, 2, True);
            Vector<Double> t3 = DOos::fileTime(names, 3, True);
            AlwaysAssertExit(t1(0) > 40000);
            AlwaysAssertExit(t2(0) > 40000);
            AlwaysAssertExit(t3(0) > 40000);

            names(0) = linkReg;
            Vector<Double> tLink = DOos::fileTime(names, 2, True);
            AlwaysAssertExit(tLink(0) > 40000);

            names(0) = missing;
            expectThrows([&]() { (void)DOos::fileTime(names, 1, True); });
        }

        {
            Vector<String> names(1);
            names(0) = regular;
            Vector<Double> sizeRegular = DOos::totalSize(names, True);
            AlwaysAssertExit(sizeRegular(0) == 5);

            names(0) = subdir;
            Vector<Double> sizeSubdir = DOos::totalSize(names, True);
            AlwaysAssertExit(sizeSubdir(0) == 2);

            names(0) = linkReg;
            Vector<Double> sizeLinkNoFollow = DOos::totalSize(names, False);
            AlwaysAssertExit(sizeLinkNoFollow(0) == 0);

            names(0) = missing;
            expectThrows([&]() { (void)DOos::totalSize(names, True); });

            AlwaysAssertExit(DOos::totalSize(missing, True) == 0);
            AlwaysAssertExit(DOos::totalSize(subdir, True) == 2);
        }

        {
            Vector<String> names(1);
            names(0) = root;
            Vector<Double> freeDir = DOos::freeSpace(names, True);
            AlwaysAssertExit(freeDir(0) > 0);

            names(0) = regular;
            Vector<Double> freeFile = DOos::freeSpace(names, True);
            AlwaysAssertExit(freeFile(0) > 0);

            names(0) = linkReg;
            Vector<Double> freeLink = DOos::freeSpace(names, False);
            AlwaysAssertExit(freeLink(0) > 0);

            names(0) = missing;
            expectThrows([&]() { (void)DOos::freeSpace(names, True); });
        }

        {
            const String copiedRegular = root + "/copied_regular.txt";
            const String copiedDir = root + "/copied_subdir";
            const String copiedLink = root + "/copied_link";

            DOos::copy(copiedRegular, regular, True, True);
            DOos::copy(copiedDir, subdir, True, True);
            DOos::copy(copiedLink, linkReg, True, False);

            AlwaysAssertExit(File(copiedRegular).isRegular());
            AlwaysAssertExit(File(copiedDir).isDirectory());
            AlwaysAssertExit(File(copiedLink).isSymLink());
            expectThrows([&]() { DOos::copy(root + "/bad_copy", missing, True, True); });
        }

        {
            const String moveSrcFile = root + "/move_src.txt";
            const String moveDstFile = root + "/move_dst.txt";
            const String moveSrcDir = root + "/move_src_dir";
            const String moveDstDir = root + "/move_dst_dir";
            const String moveSrcLink = root + "/move_src_link";
            const String moveDstLink = root + "/move_dst_link";

            writeBytes(moveSrcFile, "123");
            Directory(moveSrcDir).create();
            writeBytes(moveSrcDir + "/d.txt", "dd");
            SymLink(moveSrcLink).create("regular.txt");

            DOos::move(moveDstFile, moveSrcFile, True, True);
            DOos::move(moveDstDir, moveSrcDir, True, True);
            DOos::move(moveDstLink, moveSrcLink, True, False);

            AlwaysAssertExit(File(moveDstFile).isRegular());
            AlwaysAssertExit(!File(moveSrcFile).exists());
            AlwaysAssertExit(File(moveDstDir).isDirectory());
            AlwaysAssertExit(!File(moveSrcDir).exists());
            // Characterization: DOos::move uses SymLink::copy in symlink mode.
            AlwaysAssertExit(File(moveDstLink).isSymLink());
            AlwaysAssertExit(File(moveSrcLink).isSymLink());

            expectThrows([&]() { DOos::move(root + "/bad_move", missing, True, True); });
        }

        {
            const String removeFileA = root + "/removeA.txt";
            const String removeFileB = root + "/removeB.txt";
            const String removeLink = root + "/remove_link";
            const String removeDir = root + "/remove_dir";

            writeBytes(removeFileA, "a");
            writeBytes(removeFileB, "b");
            Directory(removeDir).create();
            writeBytes(removeDir + "/x.txt", "x");
            SymLink(removeLink).create("regular.txt");

            DOos::remove(removeFileA, False, True, True);
            AlwaysAssertExit(!File(removeFileA).exists());

            Vector<String> many(2);
            many(0) = removeFileB;
            many(1) = removeLink;
            DOos::remove(many, False, True, False);
            AlwaysAssertExit(!File(removeFileB).exists());
            AlwaysAssertExit(!File(removeLink).exists());
            AlwaysAssertExit(File(regular).exists());

            Vector<String> nonRecursive(1);
            nonRecursive(0) = removeDir;
            expectThrows([&]() { DOos::remove(nonRecursive, False, True, True); });
            AlwaysAssertExit(File(removeDir).exists());
            DOos::remove(nonRecursive, True, True, True);
            AlwaysAssertExit(!File(removeDir).exists());

            Vector<String> missingStrict(1);
            missingStrict(0) = missing;
            expectThrows([&]() { DOos::remove(missingStrict, False, True, True); });
            DOos::remove(missingStrict, False, False, True);
        }

        {
            writeBytes(root + "/table.lock", "");
            Vector<Int> info = DOos::lockInfo(root);
            AlwaysAssertExit(info.nelements() == 3);
            AlwaysAssertExit(info(2) == 0 || info(2) == 1);
        }

        removeTreeIfExists(root);
    } catch (...) {
        removeTreeIfExists(root);
        throw;
    }
    return 0;
}
