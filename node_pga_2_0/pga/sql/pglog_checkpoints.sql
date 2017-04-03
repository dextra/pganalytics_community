SELECT 
    date_trunc('minute', log_time) - ((date_part('minute', log_time)::integer % 5) * interval '1min') AS execution_time_5min,
    SUM(num_buffers) AS num_buffers,
    SUM(perc_buffers) AS perc_buffers,
    SUM(xlog_added) AS xlog_added,
    SUM(xlog_removed) AS xlog_removed,
    SUM(xlog_recycled) AS xlog_recycled,
    SUM(write_time) AS write_time,
    SUM(sync_time) AS sync_time,
    SUM(total_time) AS total_time,
    SUM(sync_files) AS sync_files,
    SUM(longest_sync_file_time) AS longest_sync_file_time,
    SUM(avg_sync_file_time) AS avg_sync_file_time
FROM mvw_pglog_checkpoint
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
GROUP BY execution_time_5min
ORDER BY execution_time_5min