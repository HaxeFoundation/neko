![NekoVM](http://nekovm.org/img/header.jpg)

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

Users of other Linux/FreeBSD distributions should build Neko from source using `make`. See [Makefile](Makefile) for additional instructions.

## Build dependencies

| library                                 | Debian/Ubuntu package                                     |
|-----------------------------------------|-----------------------------------------------------------|
| Boehm GC                                | libgc-dev                                                 |
| OpenSSL                                 | libssl-dev                                                |
| PCRE                                    | libpcre3-dev                                              |
| zlib                                    | zlib1g-dev                                                |
| Apache 2.2 / 2.4, with apr and apr-util | apache2-dev                                               |
| MariaDB / MySQL (Connector/C)           | libmariadb-client-lgpl-dev-compat (or libmysqlclient-dev) |
| SQLite                                  | libsqlite3-dev                                            |
| GTK+2                                   | libgtk2.0-dev                                             |
