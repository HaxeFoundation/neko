#
# Bootstrap errors :
#
#  pass1 = the compiler have a type error or the boot is broken
#	try to go around the problem that cause the error (the boot cannot be fixed now)
#
#  pass2 = the compiler makes a runtime error OR the boot is producing invalid code
#	try to fix the compiler
#	or try to go around the broken boot codegen
#
#  pass3 = the compiler is broken or the VM is broken
#	fix it
#


NEKOVM = nekovm
NEKOVM1 = NEKOPATH='../boot' nekovm1
NEKOVM2 = NEKOPATH='' nekovm1
TIME = @c:/progra~1/cygwin/bin/times -f %e make 

all: clean pass1 pass2 pass3

source: clean
	${NEKOVM1} nekoml/Main -n neko/Main.nml nekoml/Main.nml

pass1:
	${TIME} tpass1
	
spass1:
	${NEKOVM1} nekoml/Main -v -n neko/Main.nml nekoml/Main.nml
	${NEKOVM1} neko/Main -v *.neko neko/*.neko nekoml/*.neko

tpass1:
	${NEKOVM1} nekoml/Main neko/Main.nml nekoml/Main.nml
	
pass2:
	${TIME} tpass2

spass2:
	${NEKOVM2} nekoml/Main -v -n neko/Main.nml nekoml/Main.nml
	${NEKOVM2} neko/Main -v *.neko neko/*.neko nekoml/*.neko
	
tpass2:
	${NEKOVM2} nekoml/Main neko/Main.nml nekoml/Main.nml
	
pass3:
	${TIME} tpass3
	
spass3:
	${NEKOVM} nekoml/Main -v -n neko/Main.nml nekoml/Main.nml
	${NEKOVM} neko/Main -v *.neko neko/*.neko nekoml/*.neko

tpass3:
	${NEKOVM} nekoml/Main neko/Main.nml nekoml/Main.nml

neko: clean
	${NEKOVM1} nekoml/Main neko/Main.nml

neko3:
	${NEKOVM} nekoml/Main neko/Main.nml

nekoml: clean
	${NEKOVM1} nekoml/Main nekoml/Main.nml

install:
	-cp *.n ../boot
	-cp neko/*.n ../boot/neko
	-cp nekoml/*.n ../boot/nekoml

clean:
	rm -rf *.n
	rm -rf neko/*.n
	rm -rf nekoml/*.n

cleanall: clean
	rm -rf *.neko
	rm -rf neko/*.neko
	rm -rf nekoml/*.neko
	
.PHONY: all pass1 tpass1 pass2 tpass2 pass3 tpass3 neko nekoml install clean
