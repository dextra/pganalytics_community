SELECT
	TO_CHAR(log_time,'DD/MM/YYYY HH24:MI:SS') AS log_time,
	pages_removed,
	pages_remain,
	tuples_removed,
	tuples_remain,
	buffer_hit,
	buffer_miss,
	buffer_dirtied,
	avg_read_rate_mbs,
	avg_write_rate_mbs,
	cpu_sys_sec,
    	cpu_user_sec,
    	duration_sec
FROM mvw_pglog_autovacuum
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
    AND datname = ${database_name} AND tablename = (${schema_name}||'.'||${table_name})
ORDER BY log_time
