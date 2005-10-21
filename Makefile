### CONFIG

CFLAGS = -O3 -fPIC -fomit-frame-pointer -I vm
MAKESO = gcc -shared -WBsymbolic
LIBNEKO_LIBS = -ldl -lgc -lm
LIBNEKO = -Lbin -lneko

NEKO_EXEC = LD_LIBRARY_PATH=../bin NEKOPATH=../boot ../bin/nekovm

### MAKE

VM_OBJECTS = vm/main.o
STD_OBJECTS = libs/std/buffer.o libs/std/date.o libs/std/file.o libs/std/hash.o libs/std/init.o libs/std/int32.o libs/std/math.o libs/std/others.o libs/std/random.o libs/std/serialize.o libs/std/socket.o libs/std/sys.o libs/std/xml.o
LIBNEKO_OBJECTS = vm/alloc.o vm/builtins.o vm/callback.o vm/context.o vm/interp.o vm/load.o vm/objtable.o vm/others.o

all: libneko nekovm std compiler

libneko: bin/libneko.so

nekovm: bin/nekovm

std: bin/std.ndll

compiler:
        (cd src && ${NEKO_EXEC} Nekoml/Main Neko/Main.nml Nekoml/Main.nml)
        (cd src && ${NEKO_EXEC} Neko/Main *.neko Neko/*.neko Nekoml/*.neko)

compiler-copy:
        -mkdir bin/neko bin/neko/Neko bin/neko/Nekoml
        cp src/*.n bin/neko
        cp src/Neko/*.n bin/neko/Neko
        cp src/Nekoml/*.n bin/neko/Nekoml

bin/libneko.so: ${LIBNEKO_OBJECTS}
        ${MAKESO} ${LIBNEKO_OBJECTS} ${LIBNEKO_LIBS} -o $@

bin/nekovm: $(VM_OBJECTS)
        ${CC} ${CFLAGS} ${VM_OBJECTS} ${LIBNEKO} -o $@

bin/std.ndll: ${STD_OBJECTS}
        ${MAKESO} ${STD_OBJECTS} ${LIBNEKO} -o $@

clean:
        rm -rf bin/libneko.so bin/nekovm bin/std.ndll ${LIBNEKO_OBJECTS} ${VM_OBJECTS} ${STD_OBJECTS} bin/neko src/*.n src/*.neko src/Neko/*.n src/Neko/*.neko src/Nekoml/*.n src/Nekoml/*.neko

.SUFFIXES : .c .o

.c.o :
        ${CC} ${CFLAGS} -c $< -o $@
