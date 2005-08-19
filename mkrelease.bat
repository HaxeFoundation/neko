@echo off

tar --exclude=neko/lexer.ml --exclude=.cvsignore --exclude=neko/*.cm* --exclude=neko/*.o* --exclude=*.tgz --exclude=CVS --exclude=libs/mtypes --exclude=bin/* --exclude=*.sln --exclude=*.vcproj --exclude=*.suo --exclude=*.ncb --exclude=debug --exclude=release --exclude=libs/include -zcf neko-1.0.tgz libs neko bin vm LICENSE Makefile
mkdir neko-1.0
tar -C neko-1.0 -zxf neko-1.0.tgz
tar -zcf neko-1.0.tgz neko-1.0
rm -rf neko-1.0

pause