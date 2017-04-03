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
	ROUND(MAX(CASE WHEN s.fac = 1 THEN r.relsize END) / 1024.0 / 1024.0, 2) AS "ovw_size_mb",
	ROUND(SUM(r.relsize * s.fac) / 1024.0,2) ||' ('||ROUND(SUM(r.relsize * s.fac) / (COALESCE(NULLIF(MAX(CASE WHEN s.fac = -1 THEN r.relsize END),0),8192.0) / 100.0)::numeric,2)||'%)' AS "ovw_lag_size_kb",
	MAX(CASE WHEN s.fac = 1 THEN r.age_relfrozenxid END) AS "relfrozenxid"
FROM s
	INNER JOIN sn_relations r ON r.snap_id=s.snap_id AND r.relkind = 'r'
	INNER JOIN pm_database d ON s.database_id = d.database_id
WHERE r.relkind = 'r'
GROUP BY s.customer_id, s.server_id, s.instance_id, s.database_id, d.name, r.nspname, r.relname

