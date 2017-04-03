#!/bin/bash

. "$(dirname $0)/common.sh"

getkeys() {
	for f in "${ROOTDIR}/node_pga_2_0/pga/sql/"*.sql; do
		sed 's/\${[^}]*}/NULL/g' $f | \
		PGOPTIONS="-c search_path=ctm_demo,pganalytics,public" psql -AX -U pganalytics pganalytics 2>/dev/null | \
		head -n 1
	done | sed 's/|/\n/g' | grep -v '\(?column?\|[^a-z_]\)' | sort -u | xargs -I ? echo "    ('?'),"
}

getsqlcmd() {
	echo "WITH keys (key) AS (VALUES"
	getkeys
	echo "    (NULL)"
	echo "), langkeys AS ("
	echo "    SELECT l.code AS language, k.key"
	echo "    FROM pga_config.i18n_languages l CROSS JOIN keys k"
	echo "    WHERE k.key IS NOT NULL"
	echo ")"
	echo "INSERT INTO pga_config.i18n_translations(language, key, title)"
	echo "SELECT lk.language, lk.key, lk.key FROM langkeys lk"
	echo "WHERE NOT EXISTS(SELECT 1 FROM pga_config.i18n_translations t WHERE t.language = lk.language AND t.key = lk.key)"
}

getsqlcmd | psql -U pganalytics2 pganalytics

