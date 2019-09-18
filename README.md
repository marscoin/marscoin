
http://www.marscoin.org

http://www.marscoinfoundation.org

Copyright (c) 2009-2013 Bitcoin Developers

Copyright (c) 2011-2013 Litecoin Developers

Copyright (c) 2013-2019 Marscoin Developers

What is Marscoin?
----------------

Marscoin targets to become the blockchain for Mars, and functions as a proof of concept for incentivizing and privatizing space exploration funding.

- scrypt Litecoin-based cryptocurrency
- DarkGravityWave network protection
- confirmation block every 2 Mars minutes (123 seconds)
- subsidy halves every Mars year (668 sols)
- 721 blocks per sol
- 12.5 coins per block
- 48 million total coins by 2024
- 500k donation for non-profit MarsOne
- 500k donation for The Mars Society
- developed and supported by The Marscoin Foundation, LLC (a non-profit).

For more information, as well as an immediately useable, binary version of
the Marscoin client sofware, see http://www.marscoin.org and http://www.marscoinfoundation.org

License
-------

Marscoin is released under the terms of the MIT license. See `COPYING` for more
information or see http://opensource.org/licenses/MIT.

Development process
-------------------

Developers work in their own trees, then submit pull requests when they think
their feature or bug fix is ready.

If it is a simple/trivial/non-controversial change, then one of the Marscoin
development team members simply pulls it.

If it is a *more complicated or potentially controversial* change, then the patch
submitter will be asked to start a discussion (if they haven't already).

The patch will be accepted if there is broad consensus that it is a good thing.
Developers should expect to rework and resubmit patches if the code doesn't
match the project's coding conventions (see `doc/coding.txt`) or are
controversial.

The `master` branch is regularly built and tested, but is not guaranteed to be
completely stable. [Tags](https://github.com/marscoin/marscoin/tags) are created
regularly to indicate new official, stable release versions of Marscoin.

Testing
-------

Testing and code review is the bottleneck for development; we get more pull
requests than we can review and test. Please be patient and help out, and
remember this is a security-critical project where any mistake might cost people
lots of money.


Development tips and tricks
---------------------------

Take a look at the /doc folder and Marscoin wiki page.

**compiling for debugging**

Run configure with the --enable-debug option, then make. Or run configure with
CXXFLAGS="-g -ggdb -O0" or whatever debug flags you need.

**debug.log**

If the code is behaving strangely, take a look in the debug.log file in the data directory;
error and debugging messages are written there.

The -debug=... command-line option controls debugging; running with just -debug will turn
on all categories (and give you a very large debug.log file).

The Qt code routes qDebug() output to debug.log under category "qt": run with -debug=qt
to see it.

**testnet and regtest modes**

Run with the -testnet option to run with "play litecoins" on the test network, if you
are testing multi-machine code that needs to operate across the internet.

If you are testing something that can run on one machine, run with the -regtest option.
In regression test mode, blocks can be created on-demand; see qa/rpc-tests/ for tests
that run in -regtest mode.

**DEBUG_LOCKORDER**

Marscoin is a multithreaded application, and deadlocks or other multithreading bugs
can be very difficult to track down. Compiling with -DDEBUG_LOCKORDER (configure
CXXFLAGS="-DDEBUG_LOCKORDER -g") inserts run-time checks to keep track of which locks


Screenshot
----------

![alt text](http://www.marscoin.org/wp-content/uploads/2019/09/2019-09-18_1039.jpg "Marscoin QT client startup version 1.5")


