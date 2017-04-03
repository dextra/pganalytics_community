# Copyright 2014 Dextra, all rights reserved
# 

ISSC=wine ${HOME}/.wine/drive_c/Program\ Files\ \(x86\)/Inno\ Setup\ 5/ISCC.exe
WIN32_SETUP_DIR=packages/win32
WIN32_SETUP_FILE=pganalytics.iss

pgincludedir=`pg_config --includedir`
pglibdir=`pg_config --libdir`

all:
	# BUILD openssl (OpenSSL doesn't seem to support out of source build, so we must copy all to "build")
	mkdir -p build/libs/openssl
	test -f build/libs/openssl-build-cache || ( \
		cp -r libs/openssl-1.0.2/* build/libs/openssl/ && \
		cd build/libs/openssl/ && \
		./config no-shared && \
		make \
	)
	touch build/libs/openssl-build-cache
	# BUILD curl/libcurl
	mkdir -p build/libs/curl
	test -f build/libs/curl-build-cache || ( \
		cd build/libs/curl && \
		../../../libs/curl-7.41.0/configure \
			--disable-shared --enable-static \
			--disable-ldaps --disable-ldap \
			--disable-sspi --without-libssh2 \
			--disable-rtsp --without-libidn --without-librtmp && \
		make \
	)
	touch build/libs/curl-build-cache
	# BUILD libxml2 (libxml2 out of source build is kind of broken, so let's copy)
	# The "touch" command bellow is to avoid the error similar to: https://github.com/SIPp/sipp/issues/61
	mkdir -p build/libs/libxml2
	test -f build/libs/libxml2-build-cache || ( \
		cp -r libs/libxml2-2.9.2/* build/libs/libxml2/ && \
		cd build/libs/libxml2 && \
		touch configure.ac aclocal.m4 configure Makefile.am Makefile.in && \
		./configure --enable-shared --enable-static --with-minimum --with-push --without-lzma && \
		make \
	)
	touch build/libs/libxml2-build-cache
	# BUILD libaws
	mkdir -p build/libs/libaws
	test ! -f build/libs/libaws-build-cache && \
		cd build/libs/libaws && \
		cmake -DAWS_BUILD_STATIC_LIBRARY=on \
			-DOPENSSL_INCLUDE_DIR=/usr/include \
			-DCMAKE_CXX_FLAGS="-fpermissive" \
			-DCMAKE_BUILD_TYPE=Release \
			../../../libs/libaws/ && \
		make || true
	touch build/libs/libaws-build-cache
	# BUILD pganalytics
	mkdir -p build/
	cd build/; \
		cmake -DCMAKE_BUILD_TYPE=Debug \
		-DPQ_PATH=${pgincludedir} -DPQ_LIB=${pglibdir} \
		-DLIBAWS_PATH=`readlink -f ../libs/libaws/include/` -DLIBAWS_LIB=`readlink -f libs/libaws/src/` \
		-DCMAKE_INSTALL_PREFIX=/opt/pganalytics \
		../
	cp `readlink -f build/libs/libaws/src/libaws.so` build/src/
	${MAKE} -C build/

check:
	${MAKE} -C build/ check

pack: all
	rm -rf build/opt
	mkdir -p build/opt/pganalytics/bin build/opt/pganalytics/etc
	cp pganalytics.sample.conf build/opt/pganalytics/etc/
	cp build/src/pganalytics build/opt/pganalytics/bin/
	cp libs/backupsnapshot.sh build/opt/pganalytics/bin/
	tar cvfz build/pganalytics-`uname -m`.tar.gz --group=0 --owner=0 -C build/ opt/

win32:
	# BUILD libaws
	mkdir -p build-win32/libs/libaws
	cd build-win32/libs/libaws ; \
		cmake -DCMAKE_BUILD_TYPE=Release -DCMAKE_TOOLCHAIN_FILE=../../../cmake_addon/Toolchain-mingw32.cmake \
			-DCURL_INCLUDE_DIR=../../../libs/win32/curl/include/ -DCURL_LIB=../../../libs/win32/curl/ \
			-DOPENSSL_INCLUDE_DIR=../../../libs/win32/openssl/include/ -DOPENSSL_LIB=../../../libs/win32/openssl/lib/ \
			-DLIBXML2_INCLUDE_DIR=../../../libs/win32/libxml2-2.7.8.win32/include/ -DLIBXML2_LIB=../../../libs/win32/libxml2-2.7.8.win32/lib/ \
			-DPTHREAD_INCLUDE_DIR=../../../libs/win32/pthread/include/ -DPTHREAD_LIB=../../../libs/win32/pthread/lib/ \
			../../../libs/libaws/
	make -C build-win32/libs/libaws
	# BUILD pganalytics
	cd build-win32/; \
		cmake -DCMAKE_BUILD_TYPE=Debug -DCMAKE_TOOLCHAIN_FILE=../cmake_addon/Toolchain-mingw32.cmake \
			-DPQ_PATH=${pgincludedir} -DPQ_LIB=../libs/win32/postgresql-9.3-dlls \
			-DLIBAWS_PATH=`readlink -f ../libs/libaws/include/` -DLIBAWS_LIB=`readlink -f libs/libaws/src/` \
			-DZLIB_INCLUDE_DIR=../libs/win32/zlib/include/ -DZLIB_LIB=../libs/win32/zlib/lib/ \
			../
	${MAKE} -C build-win32/

win32-pack:
	rm -rf build-win32/pganalytics
	mkdir -p build-win32/pganalytics
	# OBS: OpenSSL and LibXML2 are already in PG pack
	cp \
		pganalytics.sample.conf \
		build-win32/src/pganalytics.exe \
		build-win32/src/win/agent/pganalytics-agent.exe \
		build-win32/libs/libaws/src/libaws.dll \
		libs/win32/pthread/lib/pthreadGC2.dll \
		libs/win32/curl/libcurl.dll \
		libs/win32/postgresql-9.3-dlls/* \
		libs/backupsnapshot.bat \
		libs/gzip.exe \
		build-win32/pganalytics/
	cd ${WIN32_SETUP_DIR}/; ${ISSC} ${WIN32_SETUP_FILE}

install:
	${MAKE} -C build/ install
	cp libs/backupsnapshot.sh /opt/pganalytics/bin/

