SELECT
	sss.datetime,
	(std.blks_hit - lag(std.blks_hit) OVER w) * 8192 / 1024 / 1024 AS "blks_hit_mb",
	(std.blks_read - lag(std.blks_read) OVER w) * 8192 / 1024 / 1024 AS "blks_read_mb"
FROM sn_stat_database std
	JOIN sn_stat_snapshot sss ON std.snap_id = sss.snap_id
WHERE sss.snap_type = 'pg_stats'
	AND customer_id = ${customer_id}
	AND server_id = ${server_id}
	AND instance_id = ${instance_id}
	AND (${database_id} = 0 OR sss.database_id = ${database_id} )
	AND sss.datetime >= ${date_from}::timestamptz - '1 hour'::interval
	AND sss.datetime < ${date_to}::timestamptz + '1 minute'::interval
	AND std.datname = ${database_name}
WINDOW w AS (ORDER BY sss.datetime)
ORDER BY sss.datetime
OFFSET 1

