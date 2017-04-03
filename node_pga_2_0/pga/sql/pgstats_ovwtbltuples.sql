WITH s AS (
	(
		SELECT DISTINCT ON(sss.customer_id, sss.server_id, sss.instance_id, sss.database_id)
			sss.customer_id, sss.server_id, sss.instance_id, sss.database_id, sss.snap_id, -1 AS fac
		FROM sn_stat_snapshot sss
		WHERE sss.snap_type = 'pg_stats'
			AND sss.customer_id = ${customer_id}
			AND sss.server_id = ${server_id}
			AND sss.instance_id = ${instance_id}
			AND (${database_id} = 0 OR sss.database_id = ${database_id} )
			AND sss.datetime >= ${date_from}::timestamptz
		ORDER BY sss.customer_id, sss.server_id, sss.instance_id, sss.database_id, sss.datetime
	)
	UNION ALL
	(
		SELECT DISTINCT ON(sss.customer_id, sss.server_id, sss.instance_id, sss.database_id)
			sss.customer_id, sss.server_id, sss.instance_id, sss.database_id, sss.snap_id, 1 AS fac
		FROM sn_stat_snapshot sss
		WHERE sss.snap_type = 'pg_stats'
			AND sss.customer_id = ${customer_id}
			AND sss.server_id = ${server_id}
			AND sss.instance_id = ${instance_id}
			AND (${database_id} = 0 OR sss.database_id = ${database_id} )
			AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
		ORDER BY sss.customer_id, sss.server_id, sss.instance_id, sss.database_id, sss.datetime DESC
	)
)
SELECT
	d.name AS database_name,
	r.nspname AS schema_name,
	r.relname AS table_name,
	COALESCE(SUM(n_tup_ins * s.fac), 0) AS "n_tup_ins",
	COALESCE(SUM(n_tup_upd * s.fac), 0) AS "n_tup_upd",
	COALESCE(SUM(n_tup_del * s.fac), 0) AS "n_tup_del",
	COALESCE(SUM(n_tup_hot_upd * s.fac), 0) AS "n_tup_hot_upd",
	COALESCE(SUM(n_live_tup * s.fac), 0) AS "n_live_tup",
	COALESCE(SUM(n_dead_tup * s.fac), 0) AS "n_dead_tup",
	COALESCE(SUM(autovacuum_count * s.fac), 0) AS "autovacuum_count"
FROM s
INNER JOIN pm_database d ON d.database_id = s.database_id
INNER JOIN sn_stat_user_tables stu ON stu.snap_id = s.snap_id
INNER JOIN sn_relations r ON r.snap_id = stu.snap_id AND r.relid = stu.relid AND r.relkind = 'r'
GROUP BY s.customer_id, s.server_id, s.instance_id, s.database_id, d.name, r.nspname, r.relname

