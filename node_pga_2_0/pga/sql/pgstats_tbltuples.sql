SELECT
	sss.datetime,
	COALESCE(n_tup_ins - lag(n_tup_ins) OVER w, 0) AS "n_tup_ins",
	COALESCE(n_tup_upd - lag(n_tup_upd) OVER w, 0) AS "n_tup_upd",
	COALESCE(n_tup_del - lag(n_tup_del) OVER w, 0) AS "n_tup_del",
	COALESCE(n_tup_hot_upd - lag(n_tup_hot_upd) OVER w, 0) AS "n_tup_hot_upd",
	COALESCE(n_live_tup - lag(n_live_tup) OVER w, 0) AS "n_live_tup",
	COALESCE(n_dead_tup - lag(n_dead_tup) OVER w, 0) AS "n_dead_tup"
FROM sn_stat_user_tables stu
	INNER JOIN sn_stat_snapshot sss ON stu.snap_id = sss.snap_id
	INNER JOIN pm_database pm ON sss.database_id = pm.database_id
	INNER JOIN sn_relations r ON r.snap_id = stu.snap_id AND r.relid = stu.relid AND r.relkind = 'r'
WHERE sss.snap_type = 'pg_stats'
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.instance_id = ${instance_id}
	AND (${database_id} = 0 OR sss.database_id = ${database_id} )
	AND sss.datetime >= ${date_from}::timestamptz - '1 hour'::interval
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND (pm.name, r.nspname, r.relname) = (${database_name},${schema_name},${table_name})
WINDOW w AS (ORDER BY sss.datetime)
ORDER BY sss.datetime
OFFSET 1

