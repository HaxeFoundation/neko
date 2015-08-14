# This Makefile works by default for Linux
# You can active other OS support by compiling with the following options
#
# For OSX
#   make os=osx
#
# For MingW/MSys
#   make os=mingw
#
# For FreeBSD
#   gmake os=freebsd
#

## CONFIG

INSTALL_PREFIX = /usr

CFLAGS = -Wall -O3 -fPIC -fomit-frame-pointer -I vm -D_GNU_SOURCE -I libs/common
EXTFLAGS = -pthread
MAKESO = $(CC) -shared -Wl,-Bsymbolic
LIBNEKO_NAME = libneko.so
LIBNEKO_LIBS = -ldl -lgc -lm
NEKOVM_FLAGS = -Lbin -lneko
STD_NDLL_FLAGS = ${NEKOVM_FLAGS} -lrt
INSTALL_FLAGS =
LIB_PREFIX = /opt/local
INSTALL_ENV =

NEKO_EXEC = ${INSTALL_ENV} LD_LIBRARY_PATH=../bin:${LD_LIBRARY_PATH} NEKOPATH=../boot:../bin ../bin/neko

# For profiling VM
#
# CFLAGS += -DNEKO_PROF

# For lower memory usage (takes more CPU !)
#
# CFLAGS += -DLOW_MEM

## MINGW SPECIFIC

ifeq (${os}, mingw)
CFLAGS = -g -Wall -O3 -momit-leaf-frame-pointer -I vm -I /usr/local/include -I libs/common
EXTFLAGS =
MAKESO = $(CC) -O -shared
LIBNEKO_NAME = neko.dll
LIBNEKO_LIBS = -Lbin -lgc
STD_NDLL_FLAGS = ${NEKOVM_FLAGS} -lws2_32
endif

### OSX SPECIFIC

ifeq (${os}, osx)
export MACOSX_DEPLOYMENT_TARGET=10.4
EXTFLAGS =
MAKESO = ${CC}
LIBNEKO_NAME = libneko.dylib
LIBNEKO_INSTALL = -install_name @executable_path/${LIBNEKO_NAME}
LIBNEKO_LIBS = -ldl ${LIB_PREFIX}/lib/libgc.a -lm -dynamiclib -single_module ${LIBNEKO_INSTALL}
NEKOVM_FLAGS = -L${CURDIR}/bin -lneko
STD_NDLL_FLAGS = -bundle -undefined dynamic_lookup ${NEKOVM_FLAGS}
CFLAGS += -L/usr/local/lib -L${LIB_PREFIX}/lib -I${LIB_PREFIX}/include
INSTALL_FLAGS = -static

endif

### FreeBSD SPECIFIC

ifeq (${os}, freebsd)
INSTALL_PREFIX = /usr/local
LIB_PREFIX = /usr/local
LIBNEKO_LIBS = -L${LIB_PREFIX}/lib -lgc-threaded -lm
CFLAGS += -I${LIB_PREFIX}/include
INSTALL_ENV = CC=cc

endif

### MAKE

VM_OBJECTS = vm/stats.o vm/main.o
STD_OBJECTS = libs/std/buffer.o libs/std/date.o libs/std/file.o libs/std/init.o libs/std/int32.o libs/std/math.o libs/std/string.o libs/std/random.o libs/std/serialize.o libs/std/socket.o libs/std/sys.o libs/std/xml.o libs/std/module.o libs/common/sha1.o libs/std/md5.o libs/std/unicode.o libs/std/utf8.o libs/std/memory.o libs/std/misc.o libs/std/thread.o libs/std/process.o
LIBNEKO_OBJECTS = vm/alloc.o vm/builtins.o vm/callback.o vm/interp.o vm/load.o vm/objtable.o vm/others.o vm/hash.o vm/module.o vm/jit_x86.o vm/threads.o

all: createbin libneko neko std compiler libs

createbin:
	-mkdir bin 2>/dev/null

libneko: bin/${LIBNEKO_NAME}

libs:
	(cd src; ${NEKO_EXEC} nekoc tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install -silent ${INSTALL_FLAGS})

tools:
	(cd src; ${NEKO_EXEC} nekoc tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install -nolibs)
	
doc:
	(cd src; ${NEKO_EXEC} nekoc tools/makedoc.neko)
	(cd src; ${NEKO_EXEC} tools/makedoc)

test:
	(cd src; ${NEKO_EXEC} nekoc tools/test.neko)
	(cd src; ${NEKO_EXEC} tools/test)

neko: bin/neko

std: bin/std.ndll

compiler:
	(cd src; ${NEKO_EXEC} nekoml -nostd neko/Main.nml nekoml/Main.nml)
	(cd src; ${NEKO_EXEC} nekoc -link ../boot/nekoc.n neko/Main)
	(cd src; ${NEKO_EXEC} nekoc -link ../boot/nekoml.n nekoml/Main)

bin/${LIBNEKO_NAME}: ${LIBNEKO_OBJECTS}
	${MAKESO} ${EXTFLAGS} -o $@ ${LIBNEKO_OBJECTS} ${LIBNEKO_LIBS}

bin/neko: $(VM_OBJECTS)
	${CC} ${CFLAGS} ${EXTFLAGS} -o $@ ${VM_OBJECTS} ${NEKOVM_FLAGS}
	strip bin/neko

bin/std.ndll: ${STD_OBJECTS}
	${MAKESO} -o $@ ${STD_OBJECTS} ${STD_NDLL_FLAGS}

clean:
	rm -rf bin/${LIBNEKO_NAME} ${LIBNEKO_OBJECTS} ${VM_OBJECTS}
	rm -rf bin/neko bin/nekoc bin/nekoml bin/nekotools
	rm -rf bin/std bin/*.ndll bin/*.n libs/*/*.o
	rm -rf src/*.n src/neko/*.n src/nekoml/*.n src/tools/*.n
	rm -rf bin/mtypes bin/tools

install:
	cp bin/${LIBNEKO_NAME} ${INSTALL_PREFIX}/lib
	cp bin/neko bin/nekoc bin/nekotools bin/nekoml bin/nekoml.std ${INSTALL_PREFIX}/bin
	-mkdir ${INSTALL_PREFIX}/lib/neko
	cp bin/*.ndll ${INSTALL_PREFIX}/lib/neko
	-mkdir ${INSTALL_PREFIX}/include
	cp vm/neko*.h ${INSTALL_PREFIX}/include
	chmod o+rx,g+rx ${INSTALL_PREFIX}/bin/neko ${INSTALL_PREFIX}/bin/nekoc ${INSTALL_PREFIX}/bin/nekotools ${INSTALL_PREFIX}/bin/nekoml ${INSTALL_PREFIX}/lib/${LIBNEKO_NAME} ${INSTALL_PREFIX}/lib/neko ${INSTALL_PREFIX}/lib/neko/*.ndll
	chmod o+r,g+r ${INSTALL_PREFIX}/bin/nekoml.std ${INSTALL_PREFIX}/include/neko*.h

uninstall:
	rm -rf ${INSTALL_PREFIX}/lib/${LIBNEKO_NAME}
	rm -rf ${INSTALL_PREFIX}/bin/neko ${INSTALL_PREFIX}/bin/nekoc ${INSTALL_PREFIX}/bin/nekotools 
	rm -rf ${INSTALL_PREFIX}/bin/nekoml ${INSTALL_PREFIX}/bin/nekoml.std
	rm -rf ${INSTALL_PREFIX}/lib/neko

.SUFFIXES : .c .o

.c.o :
	${CC} ${CFLAGS} ${EXTFLAGS} -o $@ -c $<

.PHONY: all libneko libs neko std compiler clean doc test
