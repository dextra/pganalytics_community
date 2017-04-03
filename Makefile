PGPORT=5433
PGUSER=postgres
PGPREFIX=/tmp/pgsql
PGSOURCEDIR=/tmp/postgresql-master
export PATH := ${PGPREFIX}/bin:${PATH}

ifneq ($(wildcard /dev/shm/),)
PGDATA=/dev/shm/pgdata
else
PGDATA=/tmp/pgdata
endif

.PHONY=help initdb

help:
	@echo "This is not a conventional Makefile, it is more like a set of common tasks, so here is the help:"
	@echo
	@echo " Usage: make <action> [OPTION1=value1 OPTION2=value2 ...]"
	@echo " Actions: "
	@echo " - help: this help"
	@echo " - test: run all tests"

# Download and install PostgreSQL (let's use master, because we are cool)
pginstallmaster: ${PGPREFIX}/bin/postmaster
${PGPREFIX}/bin/postmaster:
	#git clone --depth=1 git://git.postgresql.org/git/postgresql.git "${PGSOURCEDIR}/"
	wget -q https://github.com/postgres/postgres/archive/master.zip -O /tmp/master.zip
	unzip -o /tmp/master.zip -d "${PGSOURCEDIR}/"
	cd "${PGSOURCEDIR}/postgres-master"; ./configure --without-readline --prefix="${PGPREFIX}"
	${MAKE} -C "${PGSOURCEDIR}/postgres-master" install

pgtest-initdb:
	initdb -D "${PGDATA}" -U "${PGUSER}"
	echo "port = ${PGPORT}\nshared_buffers = 256MB\nsynchronous_commit = off\nfsync = off\nfull_page_writes = off\nrandom_page_cost = 1.1\ncpu_tuple_cost = 0.1\nwork_mem = 100MB\nlog_line_prefix = '%m %p/%l [%c] %q%u@%r/%d '" >> "${PGDATA}/postgresql.conf"
	pg_ctl -t 5 -l "${PGDATA}/serverlog" -wD "${PGDATA}" start
	pg_isready -p "${PGPORT}"

pgtest-stop:
	pg_ctl -D "${PGDATA}" stop -mi
pgtest-rm:
	rm -rf "${PGDATA}"
pgtest-stopandrm: pgtest-stop pgtest-rm

coletor-build:
	${MAKE} -C ./coletor/

coletor-build-win32:
	${MAKE} -C ./coletor/ win32

node-install:
	cd node_pga_2_0/; npm install

# let's make "test" an alias of "check"
test: check
check: coletor-build node-install
	./scripts/runtests.sh

