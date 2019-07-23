Copyright (c) 2009-2019 Marscoin Developers
This document updated 7/22/2019.

Distributed under the MIT/X11 software license, see the accompanying
file COPYING or http://www.opensource.org/licenses/mit-license.php.
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](http://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.

WINDOWS BUILD NOTES
====================

Below are some notes on how to build Marscoin for Windows.

The options known to work for building Marscoin on Windows are:

* On Linux using the [Mingw-w64](https://mingw-w64.org/doku.php) cross compiler tool chain. Ubuntu Bionic 16.04.6 is required
and is the platform used to build the Marscoin Windows release binaries.  See full instructions on our wiki at: https://github.com/marscoin/marscoin/wiki/Windows-Build-Notes---Ubuntu-16

Other options which may work, but which have not been extensively tested are (please contribute instructions):

* On Windows using [Windows
Subsystem for Linux (WSL)](https://msdn.microsoft.com/commandline/wsl/about) and the Mingw-w64 cross compiler tool chain.
* On Windows using a POSIX compatibility layer application such as [cygwin](http://www.cygwin.com/) or [msys2](http://www.msys2.org/).
* On Windows using a native compiler tool chain such as [Visual Studio](https://www.visualstudio.com).


Cross-compilation for Ubuntu and Windows Subsystem for Linux
------------------------------------------------------------

The steps below can be performed on Ubuntu (including in a VM). The depends system
will also work on other Linux distributions, however the commands for
installing the toolchain will be different.

First, install the general dependencies:

    sudo apt update
    sudo apt upgrade
    sudo apt install build-essential libtool automake pkg-config git

A host toolchain (`build-essential`) is necessary because some dependency
packages (such as `protobuf`) need to build host utilities that are used in the
build process.

See also: [dependencies.md](dependencies.md).

## Building for 64-bit Windows

The first step is to install the mingw-w64 cross-compilation tool chain.

    sudo apt install g++-mingw-w64-i686 mingw-w64-i686-dev g++-mingw-w64-x86-64 mingw-w64-x86-64-dev

Once the toolchain is installed the build steps are common:

Acquire the source in the usual way:

    git clone https://github.com/marscoin/marscoin.git

Once the source code is ready the build steps are below.

    cd depends
    make HOST=x86_64-w64-mingw32
    cd ..
    ./autogen.sh
    ./configure --prefix=`pwd`/depends/x86_64-w64-mingw32
    make

## Building for 32-bit Windows

To build executables for Windows 32-bit, install the following dependencies:

    sudo apt install g++-mingw-w64-i686 mingw-w64-i686-dev

Acquire the source in the usual way:

    git clone https://github.com/marscoin/marscoin.git

Then build using:

    cd depends
    make HOST=i686-w64-mingw32
    cd ..
    ./autogen.sh
    ./configure --prefix=`pwd`/depends/i686-w64-mingw32
    make

## Depends system

For further documentation on the depends system see [README.md](../depends/README.md) in the depends directory.


