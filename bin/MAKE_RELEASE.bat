@echo off

rm -rf neko-win
rm -rf neko-src

mkdir neko-win
cp neko.exe nekoc.exe nekotools.exe neko.lib gc.dll neko.dll test.n *.ndll neko-win
cp ../CHANGES ../LICENSE neko-win
mkdir neko-win\include
cp ../vm/neko.h ../vm/neko_mod.h ../vm/neko_vm.h neko-win\include

mkdir neko-src
mkdir neko-src\bin
cp -r ../boot ../libs ../src ../vm neko-src
cp ../CHANGES ../LICENSE ../Makefile ../neko.sln neko-src

cd neko-src
rm -rf */CVS */.cvsignore */*/CVS */*/.cvsignore
rm -rf */*.suo */*.ncb */*/*.suo */*/*.ncb
rm -rf */debug */release */*/debug */*/release
rm -rf libs/include libs/mod_neko/debug2 libs/mod_neko/release2 libs/regexp/regexp.o libs/std/std2.sln
rm -rf src/mtypes src/Makefile src/benchs/*.n src/*.dump src/*/*.dump

pause