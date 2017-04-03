#!/bin/sh

set -e

CONFIGFILE=$(dirname $0)/../etc/pganalytics.conf
TODAY="$( date +'%F %T%z' )"
TODAY_EPOCH=$(date -d "${TODAY}" +%s)

usage() {
  echo "
  Usage:
  ./$(basename $0) -d database_name [ -n schema ] -f /tmp/backup.dump [ -p 0001de0010 ] -i postgres -t begin|end
  "
  exit 1
}

if [ $# -lt 8 ]; then
  usage;
fi

THIS_STATSNAP_COLLECT_DIR=`cat "${CONFIGFILE}" | grep "collect_dir" | awk '{print $NF}' | sed 's/"//g'`
customer_name=`cat "${CONFIGFILE}" | grep "customer" | awk '{print $NF}' | sed 's/"//g'`
server_name=`cat "${CONFIGFILE}" | grep "server_name" | awk '{print $NF}' | sed 's/"//g'`

while getopts "n:f:d:i:p:t:hl" OPT; do
  case $OPT in
  "h") usage;;  
  "t") BACKUP_TYPE=$OPTARG; [ "x$BACKUP_TYPE" = 'xbegin' -o "x$BACKUP_TYPE" = 'xend' ] || usage ;;
  "f") BACKUP_PATH=$OPTARG;;
  "i") instance_name=$OPTARG;;
  "p") BACKUP_PART=$OPTARG;;
  "d") database_name=$OPTARG;;
  "n") schema_name=$OPTARG;;
  "?") exit -2;;
  esac
done

BACKUP_FILE=`echo ${BACKUP_PATH} | awk -F "/" '{print $NF}'`
RUNNING_BACKUP_DIR="${THIS_STATSNAP_COLLECT_DIR}/tmp"
RUNNING_BACKUP_FILE="${RUNNING_BACKUP_DIR}/${BACKUP_FILE}.running"

log() {
	echo "`date +'%F %T %Z'`: LOG: $@" >&2
}

debug() {
	echo "`date +'%F %T %Z'`: DEBUG: $@" >&2
}

import_backup_p1() {
        local snap_type='pg_dump'
        local date_begin="$( date +'%F %T%z' )"
        log "Collecting backup dump information"
        echo "# snap_type ${snap_type}"
	echo "# customer_name ${customer_name}"
	echo "# server_name ${server_name}"
	echo "# datetime ${TODAY_EPOCH}"
	echo "# real_datetime ${TODAY_EPOCH}"
	echo "# instance_name ${instance_name}"
	echo "# datname ${database_name}"
        echo
	echo "K sn_data_info data_key,data_value"
	echo "BACKUP_BEGIN	${date_begin}"
	echo "BACKUP_FILE	${BACKUP_FILE}"
	echo "BACKUP_PART	${BACKUP_PART}"
	echo "SCHEMA_NAME	${schema_name}";
}

import_backup_p2() {
        local date_end="$( date +'%F %T%z' )"
	BACKUP_SIZE=`du -b ${BACKUP_PATH} | awk '{print $1}'`
	echo "BACKUP_SIZE	${BACKUP_SIZE}"
        echo "BACKUP_END	${date_end}"
        echo "\."
}

if [ "$BACKUP_TYPE" = "begin" ]; then
	mkdir -p "${RUNNING_BACKUP_DIR}/"
	import_backup_p1 > "${RUNNING_BACKUP_FILE}"
elif [ "$BACKUP_TYPE" = "end" ]; then
	import_backup_p2 >> "${RUNNING_BACKUP_FILE}"
	mkdir -p ${THIS_STATSNAP_COLLECT_DIR}/new
	cat "${RUNNING_BACKUP_FILE}" | gzip -c > "${THIS_STATSNAP_COLLECT_DIR}/new/${TODAY_EPOCH}-000000-0000-pgdump-${database_name}.pga"
	rm "${RUNNING_BACKUP_FILE}"
fi

