#!/bin/bash

echo "[`date \"+%Y-%m-%d %H:%M:%S\"`] #### Starting purging routine ####"
psql -U pganalytics pganalytics -c "SELECT pganalytics.fn_purge_data()"
psql -U pganalytics pganalytics -c "VACUUM FULL;"
echo "[`date \"+%Y-%m-%d %H:%M:%S\"`] #### Done ####"
