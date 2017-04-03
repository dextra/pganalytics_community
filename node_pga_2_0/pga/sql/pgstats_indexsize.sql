SELECT
	sss.datetime,
	r.relsize / 1024.0 / 1024.0 AS "index_size_mb"
FROM sn_relations r
	JOIN sn_stat_snapshot sss ON r.snap_id = sss.snap_id
	JOIN pm_database pm ON sss.database_id = pm.database_id
WHERE sss.snap_type = 'pg_stats'
	AND r.relkind = 'i'
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.instance_id = ${instance_id}
	AND (${database_id} = 0 OR sss.database_id = ${database_id} )
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND (pm.name,r.nspname,r.relname) = (${database_name},${schema_name},${index_name})
ORDER BY sss.datetime
