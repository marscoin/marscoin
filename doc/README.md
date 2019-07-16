Marscoin 1.5.4
=====================

Setup
---------------------
[Marscoin](http://www.marscoin.org/buy-hold/) is the original Marscoin client and it builds the backbone of the network. However, it downloads and stores the entire history of Marscoin transactions (which is currently several GBs); depending on the speed of your computer and network connection, the synchronization process can take anywhere from a few hours to a day or more. Thankfully you only have to do this once. If you would like the process to go faster you can [download the blockchain directly](bootstrap.md).

Running
---------------------
The following are some helpful notes on how to run Marscoin on your native platform.

### Unix

You need the Qt4 run-time libraries to run Bitcoin-Qt. On Debian or Ubuntu:

	sudo apt-get install libqtgui4

Unpack the files into a directory and run:

- bin/32/marscoin-qt (GUI, 32-bit) or bin/32/marscoind (headless, 32-bit)
- bin/64/marscoin-qt (GUI, 64-bit) or bin/64/marscoind (headless, 64-bit)



### Windows

Unpack the files into a directory, and then run marscoin-qt.exe.

### OS X

Drag Bitcoin-Qt to your applications folder, and then run Bitcoin-Qt.

### Need Help?

* Ask for help on [#marscoin](http://webchat.freenode.net?channels=marscoin) on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net?channels=marscoin).
* Ask for help on the [BitcoinTalk](https://bitcointalk.org/index.php?topic=721263.0) forums

Building
---------------------
The following are developer notes on how to build Marscoin on your native platform. They are not complete guides, but include notes on the necessary libraries, compile flags, etc.

- [OS X Build Notes](build-osx.md)
- [Unix Build Notes](build-unix.md)

Development
---------------------
The Marscoin repo's [root README](https://github.com/marscoin/marscoin/blob/master/README.md) contains relevant information on the development process and automated testing.

- [Coding Guidelines](coding.md)
- [Multiwallet Qt Development](multiwallet-qt.md)
- [Release Notes](release-notes.md)
- [Release Process](release-process.md)
- [Source Code Documentation (External Link)](https://dev.visucore.com/marscoin/doxygen/)
- [Translation Process](translation_process.md)
- [Unit Tests](unit-tests.md)

### Resources
* Discuss on the [Reddit](https://reddit.com/r/marscoin) forums
* Discuss on [#marscoin-dev](http://webchat.freenode.net/?channels=marscoin) on Freenode. If you don't have an IRC client use [webchat here](http://webchat.freenode.net/?channels=marscoin-dev).
* Discuss on Discord [Discord](https://discordapp.com/channels/598732363232706570/598732363740086281)

### Miscellaneous
- [Assets Attribution](assets-attribution.md)
- [Files](files.md)
- [Tor Support](tor.md)
- [Init Scripts (systemd/upstart/openrc)](init.md)

License
---------------------
Distributed under the [MIT/X11 software license](http://www.opensource.org/licenses/mit-license.php).
This product includes software developed by the OpenSSL Project for use in the [OpenSSL Toolkit](https://www.openssl.org/). This product includes
cryptographic software written by Eric Young ([eay@cryptsoft.com](mailto:eay@cryptsoft.com)), and UPnP software written by Thomas Bernard.