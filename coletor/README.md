pgAnalytics Agent Installation
==============================

Building from Source
--------------------

Dependencies:

* cmake (for build only)
* zlib
* pthread
* libcurl (used by libaws)
* openssl (used by libaws)
* libxml2 (used by libaws)
* pcre (only for the importer)

Install dependencies using `yum`:

	$ sudo yum groupinstall "Development Tools"
	$ sudo yum install zlib-devel pcre-devel zlib-static pcre-static libstdc++-static glibc-static cmake

Install dependencies using `apt-get`:

	$ sudo apt-get install build-essential
	$ sudo apt-get install zlib1g-dev libpcre3-dev cmake

To build the binary:

	$ cd /path/to/source/coletor
	$ make

The above will build and install every binary at a dir named `build`, you can use the two binaries directly:

* `build/src/pganalytics`: is the agent, just copy `pganalytics.conf.sample` to `build/src/pganalytics.conf`, edit it and you can run;
* `build/src/backend/pganalytics-importer`: that is the importer, just run with `--help` and see the many options you have.

If you are going to install it or packing for a customer, you can run `make install`:

	$ sudo make install

Binary Distribution for Linux
-----------------------------

To generate the `.tar.gz` file:

	$ make pack

Binary Distribution for Windows
-------------------------------

To generate the .exe setup:

	$ make win32-pack

The file will be at `packages/win32/output/setup_pganalytics.exe`.

