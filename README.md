# Simple Virtual DHT Daemon

Vdht is the simple experimental framework based on DHT protocol.

## How To Build

You can build vdht from source code with following tutorial on Linux, especially on Ubuntu Linux higher than 14.04. Also you are welcome to try building vdht on other platforms.

###1. sqlite3-dev 

Be sure that sqlite3 dev library should be installed. Otherwise, run command:

```shell
$ sudo apt-get install sqlite3-dev
```

###2. miniupnpc client

Be sure that miniupnc client library should be installed. Otherwise, go to website 

```
http://miniupnp.tuxfamily.org/files/
```

and download miniupnpc source code with version higher than miniupnpc-1.9 (default with miniupnpc-1.9.tar.gz), then unzip the package:

```shell
$ tar -xzvf miniupnpc-1.9.tar.gz 
$
$ cd miniupnpc
$ make 
$ sudo make install
```

###3. Build from source

Download the vdht source code with following commands:

```shell
$ git clone https://github.com/stiartsly/vdht.git
$ cd vdht
$ make
```

## How to Be inside

There are servial shared libraries and executables.

###1. vdhtd

A simple binary to run as daemon as vdht engine.

###2. vlsctlc

A local service control command tool to communicate with vdhtd daemon.

###3. vserver/vclient

An example of pairs of service can run as service producer or demander respectively.

###3. libvdht.so

An shared library provides core vdht features.

###4. libvhdapi.so

An shared library can be integrated to client side to commnicate with vdht deamon.


## How to run

Each exacutables has it's own help information.

###1. vdht daemon

Use valid bootnode information in config file to run vdht as daemon service.

```shell
$ ./vdhtd -S
```

###2. vlsctl comand

Use vlsctlc command tool to communicate with vdhtd daemon while vdht is running:

```shell
$ ./vlsctl -s
```

## Thanks

Sinserely thanks to all teams and their projects we relies on directly or indirectly.

## License

vhdt Project source code files are made available under the MIT License, located in the LICENSE file.

