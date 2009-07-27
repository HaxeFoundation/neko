@echo off
haxe tora.hxml
rm -rf release
mkdir release
mkdir release\tora
cp haxelib.xml *.hx tora.hxml CHANGES.txt release
cp tora/*.hx release/tora
cp tora.n release/run.n
7z a -tzip release.zip release
rm -rf release
haxelib submit release.zip
pause