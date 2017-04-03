SELECT
	datname AS database_name,
	split_part(tablename,'.',1) AS schema_name,
	split_part(tablename,'.',2) AS table_name,
	COUNT(*) AS num_execution,
        SUM(pages_removed) AS pages_removed,
        SUM(pages_remain) AS pages_remain,
        SUM(tuples_removed) AS tuples_removed,
        SUM(tuples_remain) AS tuples_remain,
        SUM(buffer_hit) AS buffer_hit,
        SUM(buffer_miss) AS buffer_miss,
        SUM(buffer_dirtied) AS buffer_dirtied,
        SUM(avg_read_rate_mbs) AS avg_read_rate_mbs,
        SUM(avg_write_rate_mbs) AS avg_write_rate_mbs,
        SUM(cpu_sys_sec) AS cpu_sys_sec,
        SUM(cpu_user_sec) AS cpu_user_sec,
        SUM(duration_sec) AS duration_sec
FROM mvw_pglog_autovacuum
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
GROUP BY 1,2,3
ORDER BY num_execution DESC
