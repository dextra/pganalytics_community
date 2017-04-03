#!/bin/bash

RETENTION_DAYS=$1

usage() {
  echo "
      Exemplo: 90 dias
      ./$(basename $0) 90
"
  exit 1
}

if [ $# -ne 1 ]; then
  usage;
fi

for i in `psql -U pganalytics -Atc "SELECT nspname FROM pg_namespace WHERE nspname ~ '^ctm_'"`
do
psql -U pganalytics <<EOF
SET search_path TO ${i};
DELETE FROM sn_stat_snapshot WHERE datetime <= date_trunc('day',now() - '${RETENTION_DAYS} days'::interval);
EOF
done

vacuumdb pganalytics -U postgres --analyze

