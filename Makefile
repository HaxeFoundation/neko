## CONFIG

CFLAGS = -Wall -O3 -fPIC -fomit-frame-pointer -I vm -DCOMPACT_TABLE
EXTFLAGS = -pthread
MAKESO = $(CC) -shared -WBsymbolic
LIBNEKO_NAME = libneko.so
LIBNEKO_LIBS = -ldl -lgc -lm
NEKOVM_FLAGS = -Lbin -lneko
STD_NDLL_FLAGS = ${NEKOVM_FLAGS}
INSTALL_FLAGS =
RANLIB =

NEKO_EXEC = LD_LIBRARY_PATH=../bin:${LD_LIBRARY_PATH} NEKOPATH=../boot:../bin ../bin/neko

### INSTALLER (optional)

INSTALLER_FLAGS = -D NEKO_INSTALLER `pkg-config --cflags gtk+-2.0`
INSTALLER_API = linux
INSTALLER_LIBS = -lm -ldl /usr/lib/libgc.a /usr/lib/libpcre.a /usr/lib/libz.a `pkg-config --libs gtk+-2.0`

# For OSX
#
# MACOSX = 1

# For OSX Universal Binaries
#
# OSX_UNIVERSAL = 1


# For 64 bit
#
# CFLAGS += -D_64BITS

# For profiling VM
#
# CFLAGS += -DNEKO_PROF

# For lower memory usage (takes more CPU !)
#
# CFLAGS += -DLOW_MEM

# For MINGW/MSYS

ifeq (${WIN32}, 1)
CFLAGS = -g -Wall -O3 -momit-leaf-frame-pointer -I vm -I /usr/local/include -DCOMPACT_TABLE
EXTFLAGS =
MAKESO = $(CC) -O -shared
LIBNEKO_NAME = neko.dll
LIBNEKO_LIBS = -Lbin -lgc 
STD_NDLL_FLAGS = ${NEKOVM_FLAGS} -lws2_32
endif

### OSX SPECIFIC

ifeq (${MACOSX}, 1)
export MACOSX_DEPLOYMENT_TARGET=10.3
EXTFLAGS =
MAKESO = ${CC}
LIBNEKO_NAME = libneko.dylib
LIBNEKO_INSTALL = -install_name @executable_path/${LIBNEKO_NAME}
LIBNEKO_LIBS = -ldl -lgc -lm -dynamiclib -single_module ${LIBNEKO_INSTALL}
NEKOVM_FLAGS = -L${PWD}/bin -lneko
STD_NDLL_FLAGS = -bundle -undefined dynamic_lookup ${NEKOVM_FLAGS}

ifeq (${OSX_UNIVERSAL}, 1)

export MACOSX_DEPLOYMENT_TARGET_i386=10.4
export MACOSX_DEPLOYMENT_TARGET_ppc=10.3
CFLAGS += -arch ppc -arch i386 -L/usr/local/lib
UNIV = libs/include/osx_universal
#linking to shared lib (.a) explicitly:
LIBNEKO_DEPS = ${UNIV}/libgc.a  -lSystemStubs
LIBNEKO_LIBS = ${LIBNEKO_DEPS} -dynamiclib -single_module ${LIBNEKO_INSTALL} ${CFLAGS} 
NEKOVM_FLAGS = -L${PWD}/bin -lneko
STD_NDLL_FLAGS = -bundle ${NEKOVM_FLAGS} ${CFLAGS}
INSTALL_FLAGS = -osx-universal
RANLIB = ranlib libs/include/osx_universal/libgc.a

INSTALLER_FLAGS = -D NEKO_INSTALLER -I /opt/local/include -I /Developer/Headers/FlatCarbon
INSTALLER_API = osx
INSTALLER_LIBS = ${UNIV}/libpcre.a ${UNIV}/libz.a ${UNIV}/libgc.a /System/Library/Frameworks/Carbon.framework/Carbon

endif

endif

### MAKE

VM_OBJECTS = vm/main.o
STD_OBJECTS = libs/std/buffer.o libs/std/date.o libs/std/file.o libs/std/init.o libs/std/int32.o libs/std/math.o libs/std/string.o libs/std/random.o libs/std/serialize.o libs/std/socket.o libs/std/sys.o libs/std/xml.o libs/std/module.o libs/std/md5.o libs/std/utf8.o libs/std/memory.o libs/std/misc.o libs/std/thread.o
LIBNEKO_OBJECTS = vm/alloc.o vm/builtins.o vm/callback.o vm/context.o vm/interp.o vm/load.o vm/objtable.o vm/others.o vm/hash.o vm/module.o vm/jit_x86.o
INSTALLER_OBJECTS = libs/installer/installer.o libs/installer/api_${INSTALLER_API}.o libs/regexp/regexp.o libs/zlib/zlib.o ${STD_OBJECTS} ${LIBNEKO_OBJECTS} ${VM_OBJECTS}

all: createbin libneko neko std compiler libs

createbin:
	-mkdir bin 2>/dev/null

libneko: bin/${LIBNEKO_NAME}

libs:
	(cd src; ${NEKO_EXEC} nekoc tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install ${INSTALL_FLAGS})

doc:
	(cd src; ${NEKO_EXEC} nekoc tools/makedoc.neko)
	(cd src; ${NEKO_EXEC} tools/makedoc)

test:
	(cd src; ${NEKO_EXEC} nekoc tools/test.neko)
	(cd src; ${NEKO_EXEC} tools/test)

neko: bin/neko

std: bin/std.ndll

installer: bin/installer

compiler:
	(cd src; ${NEKO_EXEC} nekoml -v neko/Main.nml nekoml/Main.nml)
	(cd src; ${NEKO_EXEC} nekoc -link ../boot/nekoc.n neko/Main)
	(cd src; ${NEKO_EXEC} nekoc -link ../boot/nekoml.n nekoml/Main)

bin/${LIBNEKO_NAME}: ${LIBNEKO_OBJECTS}
	${RANLIB}
	${MAKESO} ${EXTFLAGS} -o $@ ${LIBNEKO_OBJECTS} ${LIBNEKO_LIBS}

bin/neko: $(VM_OBJECTS)
	${CC} ${CFLAGS} ${EXTFLAGS} -o $@ ${VM_OBJECTS} ${NEKOVM_FLAGS}
	strip bin/neko

bin/std.ndll: ${STD_OBJECTS}
	${MAKESO} -o $@ ${STD_OBJECTS} ${STD_NDLL_FLAGS}

bin/installer: $(INSTALLER_OBJECTS:.o=.o2)
	${CC} ${CFLAGS} ${EXTFLAGS} ${INSTALLER_FLAGS} -o $@ ${INSTALLER_OBJECTS:.o=.o2} ${INSTALLER_LIBS}
	strip bin/installer

clean:
	rm -rf bin/${LIBNEKO_NAME} ${LIBNEKO_OBJECTS} ${VM_OBJECTS}
	rm -rf bin/installer ${INSTALLER_OBJECTS:.o=.o2}
	rm -rf bin/neko bin/nekoc bin/nekoml bin/nekotools 
	rm -rf bin/std bin/*.ndll bin/*.n libs/*/*.o
	rm -rf src/*.n src/neko/*.n src/nekoml/*.n src/tools/*.n
	rm -rf bin/mtypes bin/tools

.SUFFIXES : .c .o .o2

.c.o :
	${CC} ${CFLAGS} ${EXTFLAGS} -o $@ -c $<

.c.o2:
	${CC} ${CFLAGS} ${EXTFLAGS} ${INSTALLER_FLAGS} -o $@ -c $<

.PHONY: all libneko libs neko std compiler clean doc test installer

