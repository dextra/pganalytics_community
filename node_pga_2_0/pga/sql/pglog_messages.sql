SELECT
	l.log_time AS datetime,
	l.pid,
	l.database_name,
	l.user_name,
	l.remote_host,
	l.error_severity,
	REPLACE(REPLACE(l.message,'<','&lt;'),'>','&gt;') AS message
FROM sn_pglog l
JOIN sn_stat_snapshot sss ON sss.snap_id = l.snap_id
LEFT JOIN pm_database d ON d.instance_id = sss.instance_id AND d.name = l.database_name
WHERE 1=1
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.instance_id = ${instance_id}
	AND (${database_id} = 0 OR d.database_id = ${database_id})
	AND (l.log_time >= ${date_from}::timestamptz - '1 minute'::interval)
	AND (l.log_time <= (${date_to}::timestamptz + '1 minute'::interval))
ORDER BY l.log_time, l.pid, l.session_line_num

