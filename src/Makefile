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


NEKOVM = neko
NEKOVM1 = NEKOPATH='../boot' c:/neko/neko1
NEKOVM2 = NEKOPATH='' c:/neko/neko1
TIME = @c:/progra~1/cygwin/bin/times -f %e make

all: clean pass1 pass2 pass3

source: clean
	${NEKOVM1} nekoml -n neko/Main.nml nekoml/Main.nml

link:
	${NEKOVM} neko/Main -link nekoc.n neko/Main
	${NEKOVM} neko/Main -link nekoml.n nekoml/Main
	${NEKOVM} tools/nekoboot nekoc.n
	${NEKOVM} tools/nekoboot nekoml.n
	${NEKOVM} tools/nekoboot nekotools.n

tools:
	${NEKOVM} neko/Main tools/test.neko
	${NEKOVM} neko/Main tools/nekoboot.neko
	${NEKOVM} nekoml/Main -p tools Tools.nml
	${NEKOVM} neko/Main -link nekotools.n Tools

pass1:
	${TIME} tpass1

spass1:
	${NEKOVM1} nekoml -v -n neko/Main.nml nekoml/Main.nml
	${NEKOVM1} nekoc -v *.neko neko/*.neko nekoml/*.neko

tpass1:
	${NEKOVM1} nekoml neko/Main.nml nekoml/Main.nml

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
	${NEKOVM} nekoml/Main neko/Main.nml nekoml/Main.nml core/*.nml -pack ../boot/nekoml.std

neko: clean
	${NEKOVM1} nekoml neko/Main.nml

neko3:
	${NEKOVM} nekoml/Main neko/Main.nml

nekoml: clean
	${NEKOVM1} nekoml nekoml/Main.nml

install: tools link
	-cp nekoc.n nekoml.n ../boot
	-cp nekoc.exe nekoml.exe nekotools.exe tools/test.n ../boot/nekoml.std ../bin

clean:
	rm -rf *.n
	rm -rf neko/*.n
	rm -rf nekoml/*.n
	rm -rf tools/*.n
	rm -rf nekoc.exe nekoml.exe nekotools.exe

cleanall: clean
	rm -rf *.neko
	rm -rf neko/*.neko
	rm -rf nekoml/*.neko

.PHONY: all pass1 tpass1 pass2 tpass2 pass3 tpass3 neko nekoml install clean link tools
