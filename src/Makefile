all:
	neko -msvc -p core -o neko neko/Main.nml nekoml/Main.nml
	neko *.neko
	(cd neko; neko *.neko)
	(cd nekoml; neko *.neko)

clean:
	rm -rf *.n *.neko neko/*.neko neko/*.n nekoml/*.neko nekoml/*.n

again: clean all
