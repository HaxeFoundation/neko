# Neko

See [Neko Website](http://nekovm.org/index)

## Compiling from Sources

### General

You need first to install [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc) and reference it in the Makefile (The flag is ```LIB_PREFIX =```).
Then run the command make command depending of your Compiler/OS.

|  Compiler/OS  | Make command  |
|---|---|
| MingW/MSys  |  make os=mingw |
| OSX  |  make os=osx |
| Unix | make |

### On OSX

You first need to install [Boehm-Demers-Weiser Garbage Collector](https://github.com/ivmai/bdwgc). 
The easiest way is using homebrew with ```brew install bdw-gc```.
By default it should install BdwGc in /usr/local/Cellar/bdw-gc/7.4.2 .

If not you need to configure ```LIB_PREFIX = /usr/local/Cellar/bdw-gc/7.4.2``` to target your installation directory.

Now you can run ```make os=osx``` and neko should compile under /bin.

