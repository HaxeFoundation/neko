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

# standard directory variables
# https://www.gnu.org/prep/standards/html_node/Directory-Variables.html#Directory-Variables
DESTDIR =
prefix = $(INSTALL_PREFIX)
exec_prefix = $(prefix)
bindir = $(exec_prefix)/bin
libdir = $(exec_prefix)/lib
includedir = $(prefix)/include

INCLUDE_FLAGS = -I vm -I libs/common
CFLAGS = -Wall -O3 -fPIC -fomit-frame-pointer -D_GNU_SOURCE -DABI_ELF
LDFLAGS =
EXTFLAGS = -pthread
MAKESO = $(CC) -shared -Wl,-Bsymbolic
LIBNEKO_NAME = libneko.so
LIBNEKO_LIBS = -ldl -lgc -lm
NEKOVM_FLAGS = -Lbin -lneko
STD_NDLL_FLAGS = ${NEKOVM_FLAGS} -lrt
INSTALL_FLAGS =
LIB_PREFIX = /opt/local

NEKO_EXEC = LD_LIBRARY_PATH=../bin:${LD_LIBRARY_PATH} NEKOPATH=../boot:../bin ../bin/neko
NEKO_BIN_LINKER_FLAGS = -Wl,-rpath,'$$ORIGIN',--enable-new-dtags

# For profiling VM
#
# CFLAGS += -DNEKO_PROF

# For lower memory usage (takes more CPU !)
#
# CFLAGS += -DLOW_MEM

# 32-bit SPECIFIC
LBITS := $(shell getconf LONG_BIT)
ifeq ($(LBITS),32)
CFLAGS += -mincoming-stack-boundary=2
endif

## MINGW SPECIFIC

ifeq (${os}, mingw)
INCLUDE_FLAGS += -I /usr/local/include
CFLAGS = -g -Wall -O3 -momit-leaf-frame-pointer
EXTFLAGS =
MAKESO = $(CC) -O -shared
LIBNEKO_NAME = neko.dll
LIBNEKO_LIBS = -Lbin -lgc
STD_NDLL_FLAGS = ${NEKOVM_FLAGS} -lws2_32
NEKO_BIN_LINKER_FLAGS =
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
CFLAGS = -Wall -O3 -fPIC -fomit-frame-pointer -D_GNU_SOURCE -L/usr/local/lib -L${LIB_PREFIX}/lib
INCLUDE_FLAGS += -I${LIB_PREFIX}/include
INSTALL_FLAGS = -static
NEKO_BIN_LINKER_FLAGS =
endif

### FreeBSD SPECIFIC

ifeq (${os}, freebsd)
INSTALL_PREFIX = /usr/local
LIB_PREFIX = /usr/local
LIBNEKO_LIBS = -L${LIB_PREFIX}/lib -lgc-threaded -lm
INCLUDE_FLAGS += -I${LIB_PREFIX}/include
INSTALL_FLAGS = -cc cc

endif

### MAKE

VM_OBJECTS = vm/stats.o vm/main.o
STD_OBJECTS = libs/std/buffer.o libs/std/date.o libs/std/file.o libs/std/init.o libs/std/int32.o libs/std/math.o libs/std/string.o libs/std/random.o libs/std/serialize.o libs/std/socket.o libs/std/sys.o libs/std/xml.o libs/std/module.o libs/common/sha1.o libs/std/md5.o libs/std/unicode.o libs/std/utf8.o libs/std/memory.o libs/std/misc.o libs/std/thread.o libs/std/process.o libs/std/elf_update.o
LIBNEKO_OBJECTS = vm/alloc.o vm/builtins.o vm/callback.o vm/elf.o vm/interp.o vm/load.o vm/objtable.o vm/others.o vm/hash.o vm/module.o vm/jit_x86.o vm/threads.o

all: createbin libneko neko std compiler libs

createbin:
	-mkdir -p bin 2>/dev/null

libneko: bin/${LIBNEKO_NAME}

libs:
	(cd src; ${NEKO_EXEC} nekoc tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install -silent ${INSTALL_FLAGS})

tools:
	(cd src; ${NEKO_EXEC} nekoc tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install -nolibs ${INSTALL_FLAGS})

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
	${MAKESO} -o $@ ${LIBNEKO_OBJECTS} ${LIBNEKO_LIBS} ${LDFLAGS} ${EXTFLAGS}

bin/neko: $(VM_OBJECTS)
	${CC} -o $@ ${VM_OBJECTS} ${NEKOVM_FLAGS} ${LDFLAGS} ${EXTFLAGS} ${NEKO_BIN_LINKER_FLAGS}

bin/std.ndll: ${STD_OBJECTS}
	${MAKESO} -o $@ ${STD_OBJECTS} ${STD_NDLL_FLAGS} ${LDFLAGS} ${EXTFLAGS}

clean:
	rm -rf bin/${LIBNEKO_NAME} ${LIBNEKO_OBJECTS} ${VM_OBJECTS}
	rm -rf bin/neko bin/nekoc bin/nekoml bin/nekotools
	rm -rf bin/std bin/*.ndll bin/*.n libs/*/*.o
	rm -rf src/*.n src/neko/*.n src/nekoml/*.n src/tools/*.n
	rm -rf bin/mtypes bin/tools

install:
	cp bin/$(LIBNEKO_NAME) $(DESTDIR)$(libdir)
	cp bin/neko bin/nekoc bin/nekotools bin/nekoml bin/nekoml.std $(DESTDIR)$(bindir)
	-mkdir -p $(DESTDIR)$(libdir)/neko
	cp bin/*.ndll $(DESTDIR)$(libdir)/neko
	-mkdir -p $(DESTDIR)$(includedir)
	cp vm/neko*.h $(DESTDIR)$(includedir)
	chmod o+rx,g+rx $(DESTDIR)$(bindir)/neko $(DESTDIR)$(bindir)/nekoc $(DESTDIR)$(bindir)/nekotools $(DESTDIR)$(bindir)/nekoml
	chmod o+r,g+r $(DESTDIR)$(libdir)/$(LIBNEKO_NAME) $(DESTDIR)$(libdir)/neko/*.ndll $(DESTDIR)$(bindir)/nekoml.std $(DESTDIR)$(includedir)/neko*.h

install-strip: install
	strip $(DESTDIR)$(bindir)/neko
	strip $(DESTDIR)$(libdir)/$(LIBNEKO_NAME)
	strip $(DESTDIR)$(bindir)/nekoc $(DESTDIR)$(bindir)/nekoml $(DESTDIR)$(bindir)/nekotools $(DESTDIR)$(bindir)/*.ndll

uninstall:
	rm -rf $(DESTDIR)$(libdir)/$(LIBNEKO_NAME)
	rm -rf $(DESTDIR)$(bindir)/neko $(DESTDIR)$(bindir)/nekoc $(DESTDIR)$(bindir)/nekotools 
	rm -rf $(DESTDIR)$(bindir)/nekoml $(DESTDIR)$(bindir)/nekoml.std
	rm -rf $(DESTDIR)$(libdir)/neko

package:
	git archive -o bin/neko-`(cd bin; ${NEKO_EXEC} -version)`.tar.gz HEAD

.SUFFIXES : .c .o

.c.o :
	${CC} ${INCLUDE_FLAGS} ${CFLAGS} ${EXTFLAGS} -o $@ -c $<

.PHONY: all libneko libs neko std compiler clean doc test
