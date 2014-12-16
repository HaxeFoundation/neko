# Neko

See (Neko Websit)[http://nekovm.org/index]

## Compiling from Sources

### On OSX

You first need to install (Boehm-Demers-Weiser Garbage Collector)[https://github.com/ivmai/bdwgc]. 
The easiest way is using homebrew with ```brew install bdw-gc```.
By default it should install BdwGc in /usr/local/Cellar/bdw-gc/7.4.2 .

If not you need to configure ```LIB_PREFIX = /usr/local/Cellar/bdw-gc/7.4.2``` to target your installation directory.

Now you can run ```make os=osx``` and neko should compile under /bin.

