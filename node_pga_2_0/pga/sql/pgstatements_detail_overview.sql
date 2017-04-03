SELECT
    date_trunc('minute', e.log_time) - ((date_part('minute', e.log_time)::integer % 5) * interval '1min') AS execution_time_10min,
    avg(e.duration / 1000.0) AS avg_duration_s,
    min(e.duration / 1000.0) AS min_duration_s,
    max(e.duration / 1000.0) AS max_duration_s,
    count(*) AS num_execution
FROM sn_statements_executed e
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (${database_id} = 0 OR database_id = ${database_id})
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
    AND e.statement_id = ${statement_id}
GROUP BY execution_time_10min
ORDER BY execution_time_10min

