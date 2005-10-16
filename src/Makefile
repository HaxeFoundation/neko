all:
	rm -rf Main.neko
	neko -msvc -p core -p neko -o neko Main.nml
	mv neko/Main.neko ./
	neko *.neko

clean:
	rm -rf *.n *.neko

again: clean all