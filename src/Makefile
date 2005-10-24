
NEKOVM = nekovm
NEKOVM1 = NEKOPATH='../boot' nekovm1
NEKOVM2 = NEKOPATH='' nekovm1
TIME = c:/progra~1/cygwin/bin/times -f %E make 

all: clean pass1 pass2 pass3

pass1:
	${TIME} tpass1
	
tpass1:
	${NEKOVM1} nekoml/Main neko/Main.nml nekoml/Main.nml
	${NEKOVM1} neko/Main *.neko neko/*.neko nekoml/*.neko
	
pass2:
	${TIME} tpass2
	
tpass2:
	${NEKOVM2} nekoml/Main neko/Main.nml nekoml/Main.nml
	${NEKOVM2} neko/Main *.neko neko/*.neko nekoml/*.neko
	
pass3:
	${TIME} tpass3
	
tpass3:
	${NEKOVM} nekoml/Main neko/Main.nml nekoml/Main.nml
	${NEKOVM} neko/Main *.neko neko/*.neko nekoml/*.neko

neko: clean
	${NEKOVM1} nekoml/Main neko/Main.nml
	${NEKOVM1} neko/Main *.neko neko/*.neko

neko3:
	${NEKOVM} nekoml/Main neko/Main.nml
	${NEKOVM} neko/Main *.neko neko/*.neko

nekoml: clean
	${NEKOVM1} nekoml/Main nekoml/Main.nml
	${NEKOVM1} neko/Main *.neko nekoml/*.neko neko/*.neko 

install:
	-cp *.n ../boot
	-cp neko/*.n ../boot/neko
	-cp nekoml/*.n ../boot/nekoml

clean:
	rm -rf neko/*.neko neko/*.n
	rm -rf nekoml/*.neko nekoml/*.n
	rm -rf *.n *.neko

.PHONY: all pass1 tpass1 pass2 tpass2 pass3 tpass3 neko nekoml install clean
