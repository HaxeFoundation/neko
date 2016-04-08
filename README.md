![NekoVM](https://cloud.githubusercontent.com/assets/576184/14234981/10528a0e-f9f1-11e5-8922-894569b2feea.png)

[![TravisCI Build Status](https://travis-ci.org/HaxeFoundation/neko.svg?branch=master)](https://travis-ci.org/HaxeFoundation/neko)
[![AppVeyor Build Status](https://ci.appveyor.com/api/projects/status/github/HaxeFoundation/neko?branch=master&svg=true)](https://ci.appveyor.com/project/HaxeFoundation/neko)

# Neko Virtual Machine

See http://nekovm.org/

## Snapshot Builds

### Windows

Compiled binaries can be found in the "artifacts" tab of each AppVeyor build. The most recent one is
https://ci.appveyor.com/project/HaxeFoundation/neko/build/artifacts

Note that you may need to install [Microsoft Visual C++ 2010 Redistributable Package (x86)](https://www.microsoft.com/en-us/download/details.aspx?id=5555), in case it complains "MSVCR100.dll" is missing.

### Mac

Neko snapshot of the latest master branch can be built using [homebrew](http://brew.sh/) in a single command: `brew install neko --HEAD`. It will install required dependencies, build, and install Neko to the system. The binaries can be found at `brew --prefix neko`.

Use `brew reinstall haxe --HEAD` to upgrade in the future.

### Linux

Ubuntu users can use the [Haxe Foundation snapshots PPA](https://launchpad.net/~haxe/+archive/ubuntu/snapshots) to install a Neko package built from the latest master branch. To do so, run the commands as follows:
```
sudo add-apt-repository ppa:haxe/snapshots -y
sudo apt-get update
sudo apt-get install neko -y
```

Users of other Linux/FreeBSD distributions should build Neko from source. See below for additional instructions.

## Build dependencies

| library / tool                          | OS          | Debian/Ubuntu package                                     |
|-----------------------------------------|-------------|-----------------------------------------------------------|
| Visual Studio 2010                      | Windows     |                                                           |
| XCode (with its "Command line tools")   | Mac         |                                                           |
| gcc                                     | Linux       | build-essential                                           |
| pkg-config                              | Mac/Linux   | pkg-config                                                |
| cmake                                   | all         | cmake                                                     |
| Boehm GC                                | all         | libgc-dev                                                 |
| OpenSSL                                 | all         | libssl-dev                                                |
| PCRE                                    | all         | libpcre3-dev                                              |
| zlib                                    | all         | zlib1g-dev                                                |
| Apache 2.2 / 2.4, with apr and apr-util | all         | apache2-dev                                               |
| MariaDB / MySQL (Connector/C)           | all         | libmariadb-client-lgpl-dev-compat (or libmysqlclient-dev) |
| SQLite                                  | all         | libsqlite3-dev                                            |
| GTK+2                                   | Mac/Linux   | libgtk2.0-dev                                             |
| mbed TLS                                | Mac/Linux   | libmbedtls-dev                                            |

## Build instructions

### Mac/Linux

```shell
# make a build directory, and change to it
mkdir build
cd build

# run cmake
cmake ..

# let's build, the outputs can be located in the "bin" directory
make

# install it if you want
make install
```

### Windows

Below is the instructions of building Neko in a Visual Studio command prompt.
You may use the CMake GUI and Visual Studio to build it instead.

```shell
# make a build directory, and change to it
mkdir build
cd build

# run cmake
cmake -G "Visual Studio 10 2010" ..

# let's build, the outputs can be located in the "bin" directory
msbuild ALL_BUILD.vcxproj
```