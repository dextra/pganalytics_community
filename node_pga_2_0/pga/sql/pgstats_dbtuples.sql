SELECT
	sss.datetime,
	COALESCE(tup_inserted - lag(tup_inserted) OVER w, 0) AS "tup_inserted",
	COALESCE(tup_updated - lag(tup_updated) OVER w, 0) AS "tup_updated",
	COALESCE(tup_deleted - lag(tup_deleted) OVER w, 0) AS "tup_deleted"
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
WINDOW w AS (ORDER BY date_trunc('hour', sss.datetime))
ORDER BY sss.datetime
OFFSET 1

