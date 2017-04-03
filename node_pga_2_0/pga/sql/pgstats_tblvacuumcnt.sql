SELECT
sss.datetime,
	COALESCE(r.relfrozenxid::text::bigint - lag(r.relfrozenxid::text::bigint) OVER w, 0) AS "relfrozenxid",
	COALESCE(vacuum_count - lag(vacuum_count) OVER w, 0) AS "vacuum_count",
	COALESCE(autovacuum_count - lag(autovacuum_count) OVER w, 0) AS "autovacuum_count",
	COALESCE(analyze_count - lag(analyze_count) OVER w, 0) AS "analyze_count",
	COALESCE(autoanalyze_count - lag(autoanalyze_count) OVER w, 0) AS "autoanalyze_count"
FROM sn_stat_user_tables stu
	INNER JOIN sn_stat_snapshot sss ON stu.snap_id = sss.snap_id
	INNER JOIN pm_database pm ON sss.database_id = pm.database_id
	INNER JOIN sn_relations r ON r.snap_id = stu.snap_id AND r.relid = stu.relid AND r.relkind = 'r'
WHERE sss.snap_type = 'pg_stats'
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.instance_id = ${instance_id}
	AND (${database_id} = 0 OR sss.database_id = ${database_id} )
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND (pm.name, r.nspname, r.relname) = (${database_name},${schema_name},${table_name})
WINDOW w AS (ORDER BY sss.datetime)
ORDER BY sss.datetime
OFFSET 1

