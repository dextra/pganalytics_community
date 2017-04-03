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
	d.name AS "database_name",
	ridx.nspname AS schema_name,
	rtbl.relname AS table_name,
	ridx.relname AS index_name,
	MAX(CASE WHEN s.fac = 1 THEN stu.idx_scan END) AS "idx_scan",
	COALESCE(SUM(stu.idx_scan * s.fac), 0) ||' ('||ROUND(COALESCE(SUM(stu.idx_scan * s.fac), 0) / (NULLIF(MAX(CASE WHEN s.fac = -1 THEN stu.idx_scan END),0) / 100.00),2) ||'%)' AS "lag_idx_scan_perc"
FROM s
INNER JOIN pm_database d ON d.database_id = s.database_id
INNER JOIN sn_stat_user_indexes stu ON stu.snap_id = s.snap_id
INNER JOIN sn_relations ridx ON ridx.snap_id = stu.snap_id AND ridx.relid = stu.indexrelid
INNER JOIN sn_relations rtbl ON rtbl.snap_id = stu.snap_id AND rtbl.relid = stu.relid
GROUP BY s.customer_id, s.server_id, s.instance_id, s.database_id, d.name, ridx.relname,schema_name,table_name 

