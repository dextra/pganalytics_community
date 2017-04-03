SELECT DISTINCT
	sd.datname AS "database_name",
	(last_value(dbsize) OVER w) / 1024 / 1024 AS "ovw_size_mb",
	((last_value(dbsize) OVER w) - (first_value(dbsize) OVER w)) / 1024 / 1024 ||
	' ('|| ROUND(((last_value(dbsize) OVER w) - (first_value(dbsize) OVER w)) / ((first_value(dbsize) OVER w) / 100.00),2) ||' %)' AS "ovw_lag_size_mb",
	ROUND(AVG(blks_hit / (coalesce(nullif((blks_read + blks_hit),0),1) / 100.00)) OVER w,2) AS "ovw_blks_hit_perc",
	(last_value(deadlocks) OVER w) || ' (' ||
	((last_value(deadlocks) OVER w) - (first_value(deadlocks) OVER w))||')' AS "ovw_deadlocks",
	(last_value(conflicts) OVER w) || ' (' ||
	((last_value(conflicts) OVER w) - (first_value(conflicts) OVER w))||')' AS "ovw_conflicts",
	(last_value(age_datfrozenxid) OVER w) || ' (' ||
	((last_value(age_datfrozenxid) OVER w) - (first_value(age_datfrozenxid) OVER w))||')' AS "ovw_datfrozenxid",
	(last_value(temp_bytes) OVER w) / 1024 / 1024 || ' (' ||
	((last_value(temp_bytes) OVER w) - (first_value(temp_bytes) OVER w)) / 1024 / 1024 ||')' AS "ovw_temp_bytes_mb"
FROM sn_database sd
	JOIN sn_stat_snapshot sss ON sd.snap_id = sss.snap_id
	JOIN sn_stat_database std ON sss.snap_id = std.snap_id
WHERE sss.snap_type = 'pg_stats'
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.instance_id = ${instance_id}
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
WINDOW w AS (PARTITION BY sd.datname ORDER BY sss.datetime ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING)

