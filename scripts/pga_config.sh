#!/bin/bash

. "$(dirname $0)/common.sh"

pga_config_file="${ROOTDIR}/data_model/pga_config.sql"
pganalytics_file="${ROOTDIR}/data_model/pga_pganalytics.sql"

pull() {
	psql -U pganalytics pganalytics -c "DROP TABLE IF EXISTS pganalytics2.widgets;"
	{
		echo "GRANT CREATE ON DATABASE pganalytics TO pganalytics;"
		echo "ALTER DEFAULT PRIVILEGES FOR ROLE pganalytics IN SCHEMA pganalytics GRANT REFERENCES ON TABLES TO pganalytics;"
		echo "GRANT REFERENCES ON ALL TABLES IN SCHEMA pganalytics TO pganalytics;"
	} | psql -U pganalytics pganalytics
	psql --set=ON_ERROR_STOP=on --single-transaction -U pganalytics pganalytics -f "${pga_config_file}"
}

commit() {
	{
		echo '\set ON_ERROR_STOP on'
		echo '\set AUTOCOMMIT off'
		echo 'DROP SCHEMA IF EXISTS pga_config CASCADE;'
		pg_dump -U pganalytics pganalytics -n pga_config
	} > "${pga_config_file}"
	{
		echo '\set ON_ERROR_STOP on'
                echo '\set AUTOCOMMIT off'
		echo 'DROP SCHEMA IF EXISTS pganalytics CASCADE;'
		pg_dump -U pganalytics pganalytics -n pganalytics -T log_imports -T log_imports_error -T alert
	} > "${pganalytics_file}"
}

usage() {
	echo "$0 { pull | commit }"
	echo
	echo "  pull: load pga_config.sql into local PostgreSQL"
	echo "  commit: save local PostgreSQL pga_config schema into pga_config.sql (doesn't add to git)"
}

if [ "$1" = "pull" ]; then
	pull
elif [ "$1" = "commit" ]; then
	commit
else
	usage
fi

