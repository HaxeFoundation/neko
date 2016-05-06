![NekoVM](https://cloud.githubusercontent.com/assets/576184/14234981/10528a0e-f9f1-11e5-8922-894569b2feea.png)

[![TravisCI Build Status](https://travis-ci.org/HaxeFoundation/neko.svg?branch=master)](https://travis-ci.org/HaxeFoundation/neko)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/HaxeFoundation/neko?branch=master&svg=true)](https://ci.appveyor.com/project/HaxeFoundation/neko)

# Neko Virtual Machine

See http://nekovm.org/

## Snapshot Builds

### Windows

Compiled binaries can be found in the "artifacts" tab of each [AppVeyor build](https://ci.appveyor.com/project/HaxeFoundation/neko/history).

### Mac

Neko snapshot of the latest master branch can be built using [homebrew](http://brew.sh/) in a single command: `brew install neko --HEAD`. It will install required dependencies, build, and install Neko to the system. The binaries can be found at `brew --prefix neko`.

Use `brew reinstall neko --HEAD` to upgrade in the future.

### Linux

Ubuntu users can use the [Haxe Foundation snapshots PPA](https://launchpad.net/~haxe/+archive/ubuntu/snapshots) to install a Neko package built from the latest master branch. To do so, run the commands as follows:
```
sudo add-apt-repository ppa:haxe/snapshots -y
sudo apt-get update
sudo apt-get install neko -y
```

Users of other Linux/FreeBSD distributions should build Neko from source. See below for additional instructions.

## Build instruction

Neko can be built using CMake (version 3.x is recommended) and one of the C compilers listed as follows:

 * Windows: Visual Studio 2010 / 2013 (Visual Studio 2015 is not yet supported)
 * Mac: XCode (with its "Command line tools")
 * Linux: gcc (can be obtained by installing the "build-essential" Debian/Ubuntu package)

Neko needs to link with various third-party libraries, which are summerized as follows:

| library / tool                          | OS          | Debian/Ubuntu package                                     |
|-----------------------------------------|-------------|-----------------------------------------------------------|
| Boehm GC                                | all         | libgc-dev                                                 |
| OpenSSL                                 | all         | libssl-dev                                                |
| PCRE                                    | all         | libpcre3-dev                                              |
| zlib                                    | all         | zlib1g-dev                                                |
| Apache 2.2 / 2.4, with apr and apr-util | all         | apache2-dev                                               |
| MariaDB / MySQL (Connector/C)           | all         | libmariadb-client-lgpl-dev-compat (or libmysqlclient-dev) |
| SQLite                                  | all         | libsqlite3-dev                                            |
| mbed TLS                                | all         | libmbedtls-dev                                            |
| GTK+2                                   | Linux       | libgtk2.0-dev                                             |

On Windows, CMake will automatically download and build the libraries in the build folder during the build process. However, you need to install [Perl](http://www.activestate.com/activeperl) manually because OpenSSL needs it for configuration. On Mac/Linux, you should install the libraries manaully to your system before building Neko, or use the `STATIC_DEPS` CMake option, which will be explained in [CMake options](#cmake-options).

### Building on Mac/Linux

```shell
# make a build directory, and change to it
mkdir build
cd build

# run cmake
cmake ..

# let's build, the outputs can be located in the "bin" directory
make

# install it if you want
# default installation prefix is /usr/local
make install
```

### Building on Windows

Below is the instructions of building Neko in a Visual Studio command prompt.
You may use the CMake GUI and Visual Studio to build it instead.

```shell
# make a build directory, and change to it
mkdir build
cd build

# run cmake
cmake -G "Visual Studio 12 2013" ..

# let's build, the outputs can be located in the "bin" directory
msbuild ALL_BUILD.vcxproj /p:Configuration=Release

# install it if you want
# default installation location is C:\HaxeToolkit\neko
msbuild INSTALL.vcxproj /p:Configuration=Release
```

### CMake options

A number of options can be used to customize the build. They can be specified in the CMake GUI, or passed to `cmake` in command line as follows:

```shell
cmake "-Doption=value" ..
```

#### `WITH_NDLLS`

Available on all platforms. Default value: `std.ndll;zlib.ndll;mysql.ndll;mysql5.ndll;regexp.ndll;sqlite.ndll;ui.ndll;mod_neko2.ndll;mod_tora2.ndll;ssl.ndll`

It defines the ndll files to be built. You may remove ndlls from this list, such that you can avoid installing/building some dependencies.

#### `STATIC_DEPS`

Available on Mac/Linux. Default value: `none`

It defines the dependencies that should be linked statically. Can be `all`, `none`, or a list of library names (e.g. `BoehmGC;Zlib;OpenSSL;MariaDBConnector;PCRE;Sqlite3;APR;APRutil;Apache;MbedTLS`).

CMake will automatically download and build the specified dependencies into the build folder. If a library is not present in this list, it should be installed manually, and it will be linked dynamically.

All third-party libraries, except GTK+2 (Linux), can be linked statically. We do not support statically linking GTK+2 due to the diffculty of building it and its own dependencies.

#### `RELOCATABLE`

Available on Mac/Linux. Default value: `ON`

Set RPATH to `$ORIGIN` (Linux) / `@executable_path` (Mac). It allows the resulting Neko VM executable to locate libraries (e.g. "libneko" and ndll files) in its local directory, such that the libraries need not be installed to "/usr/lib" or "/usr/local/lib".

#### `RUN_LDCONFIG`

Available on Linux. Default value: `ON`

Whether to run `ldconfig` automatically after `make install`. It is for refreshing the shared library cache such that "libneko" can be located correctly by the Neko VM.
