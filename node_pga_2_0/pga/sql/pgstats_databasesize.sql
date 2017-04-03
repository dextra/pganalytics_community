SELECT
sss.datetime,
sd.dbsize / 1024 / 1024 AS "database_size_mb"
FROM sn_database sd
JOIN sn_stat_snapshot sss ON sd.snap_id = sss.snap_id
WHERE sss.snap_type = 'pg_stats'
AND customer_id = ${customer_id}
AND server_id = ${server_id}
AND instance_id = ${instance_id}
AND (${database_id} = 0 OR sss.database_id = ${database_id} )
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
AND sd.datname = ${database_name}
ORDER BY sss.datetime
