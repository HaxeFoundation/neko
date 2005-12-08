### CONFIG

CFLAGS = -Wall -O3 -fPIC -fomit-frame-pointer -I vm -DCOMPACT_TABLE
MAKESO = gcc -shared -WBsymbolic
LIBNEKO_LIBS = -ldl -lgc -lm
LIBNEKO = -Lbin -lneko

NEKO_EXEC = LD_LIBRARY_PATH=../bin:${LD_LIBRARY_PATH} NEKOPATH=../boot ../bin/nekovm

# For 64 bit
#
# CFLAGS += -D_64BITS

# For OSX
#
# MAKESO = gcc -dynamic
# LIBNEKO = 

# For profiling VM
#
# CFLAGS += -DNEKO_PROF

# For lower memory usage (takes more CPU !)
#
# CFLAGS += -DLOW_MEM

### MAKE

VM_OBJECTS = vm/main.o
STD_OBJECTS = libs/std/buffer.o libs/std/date.o libs/std/file.o libs/std/init.o libs/std/int32.o libs/std/math.o libs/std/others.o libs/std/random.o libs/std/serialize.o libs/std/socket.o libs/std/sys.o libs/std/xml.o libs/std/module.o libs/std/md5.o libs/std/utf8.o
LIBNEKO_OBJECTS = vm/alloc.o vm/builtins.o vm/callback.o vm/context.o vm/interp.o vm/load.o vm/objtable.o vm/others.o vm/hash.o vm/module.o

all: libneko nekovm std compiler libs

libneko: bin/libneko.so

libs:
	(cd src; ${NEKO_EXEC} neko tools/install.neko)
	(cd src; ${NEKO_EXEC} tools/install)

doc:
	(cd src; ${NEKO_EXEC} neko tools/makedoc.neko)
	(cd src; ${NEKO_EXEC} tools/makedoc)
	
test:
	(cd src; ${NEKO_EXEC} neko tools/test.neko)
	(cd src; ${NEKO_EXEC} tools/test)

nekovm: bin/nekovm

std: bin/std.ndll

compiler:
	(cd src; ${NEKO_EXEC} nekoml -v neko/Main.nml nekoml/Main.nml)
	(cd src; ${NEKO_EXEC} neko -link ../bin/neko neko/Main)
	(cd src; ${NEKO_EXEC} neko -link ../bin/nekoml nekoml/Main)

bin/libneko.so: ${LIBNEKO_OBJECTS}
	${MAKESO} -o $@ ${LIBNEKO_OBJECTS} ${LIBNEKO_LIBS}

bin/nekovm: $(VM_OBJECTS)
	${CC} ${CFLAGS} -o $@ ${VM_OBJECTS} ${LIBNEKO}

bin/std.ndll: ${STD_OBJECTS}
	${MAKESO} -o $@ ${STD_OBJECTS} ${LIBNEKO}

clean:
	rm -rf bin/libneko.so bin/nekovm ${LIBNEKO_OBJECTS} ${VM_OBJECTS}
	rm -rf bin/std bin/*.ndll libs/*/*.o
	rm -rf src/*.n src/neko/*.n src/nekoml/*.n src/tools/*.n

.SUFFIXES : .c .o

.c.o :
	${CC} ${CFLAGS} -o $@ -c $<

.PHONY: all libneko libs nekovm std compiler clean doc test
