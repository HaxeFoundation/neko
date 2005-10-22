
NEKOVM = NEKOPATH='${NEKOPATH};../boot' nekovm

all:
	${NEKOVM} nekoml/Main neko/Main.nml nekoml/Main.nml
	${NEKOVM} neko/Main *.neko neko/*.neko nekoml/*.neko

neko:
	${NEKOVM} nekoml/Main neko/Main.nml
	${NEKOVM} neko/Main *.neko neko/*.neko

nekoml:
	${NEKOVM} nekoml/Main nekoml/Main.nml
	${NEKOVM} neko/Main *.neko nekoml/*.neko neko/*.neko 

install:
	cp *.n ../boot
	cp neko/*.n ../boot/neko
	cp nekoml/*.n ../boot/nekoml

clean:
	rm -rf neko/*.neko neko/*.n
	rm -rf nekoml/*.neko nekoml/*.n
	rm -rf *.n *.neko

.PHONY: all neko nekoml install clean
