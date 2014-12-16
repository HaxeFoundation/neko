# Neko

See [Neko Website](http://nekovm.org/index)

## Compiling from Sources

### General Worflow

You need first to install [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc) and reference it in the Makefile (The flag is ```LIB_PREFIX =```).
Then run the command make command depending of your Compiler/OS.

|  Compiler/OS  | Make command  |
|---|---|
| MingW/MSys  |  make os=mingw |
| OSX  |  make os=osx |
| Unix | make |

### On Windows

Compiling for Windows from sources is possible using the Visual Studio project files. 

You need to compile the neko.sln project (nekovm and nekovm_dll only) as well as the libs/libs.sln project.

### On OSX

You first need to install [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc). 
The easiest way is using homebrew with ```brew install bdw-gc```.

By default it should install BdwGc in /usr/local/Cellar/bdw-gc/7.4.2 .

If not you need to configure ```LIB_PREFIX``` to target your installation directory.

```Makefile
ifeq (${os}, osx)
export MACOSX_DEPLOYMENT_TARGET=10.4
...
LIB_PREFIX = /usr/local/Cellar/bdw-gc/7.4.2
...
endif
```

Now you can run ```make os=osx``` and neko should compile under /bin.

### On Unix

Depending on you Distribution there is generally a package for installing [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc).

| Distribution | Package info |
|---|---|
| Debian | [libgc-dev](https://packages.debian.org/fr/sid/libgc-dev) |
| Fedora | [gc](http://rpmfind.net/linux/rpm2html/search.php?query=gc) |

Verify the ```LIB_PREFIX``` in the Makefile, it should target the directory where you install the [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc).

Then you can ran ```make``` to compile neko.