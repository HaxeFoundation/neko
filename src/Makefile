all:
	neko -msvc -p core -o neko neko/Main.nml nekoml/Main.nml
	neko *.neko
	(cd neko; neko *.neko)
	(cd nekoml; neko *.neko)

rec:
	nekovm neko/Main *.neko
	nekovm neko/Main neko/*.neko
	nekovm neko/Main nekoml/*.neko

clean:
	rm -rf *.n *.neko neko/*.neko neko/*.n nekoml/*.neko nekoml/*.n

again: clean all
