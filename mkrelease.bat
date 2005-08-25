@echo off

set VERSION=neko-1.0.5
set FILES=libs neko bin vm mk LICENSE INSTALL CHANGES Makefile configure

tar --exclude=neko/lexer.ml --exclude=.cvsignore --exclude=neko/*.cm* --exclude=neko/*.o* --exclude=*.tgz --exclude=CVS --exclude=libs/mtypes --exclude=bin/* --exclude=*.sln --exclude=*.vcproj --exclude=*.suo --exclude=*.ncb --exclude=debug --exclude=release --exclude=libs/include -zcf %VERSION%.tgz %FILES%
mkdir %VERSION%
tar -C %VERSION% -zxf %VERSION%.tgz
tar -zcf %VERSION%.tgz %VERSION%
rm -rf %VERSION%

pause