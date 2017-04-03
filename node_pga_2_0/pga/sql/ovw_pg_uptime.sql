WITH ss AS (
    SELECT ss1.snap_id, ss1.datetime, coalesce(ss1.real_datetime, ss1.datetime) AS real_datetime
		FROM sn_stat_snapshot ss1
		WHERE ss1.snap_type = 'pg_stats'
			AND (${customer_id} = 0 OR ss1.customer_id = ${customer_id} )
			AND (${server_id} = 0 OR ss1.server_id = ${server_id} )
			AND (${instance_id} = 0 OR ss1.instance_id = ${instance_id} )
			AND ss1.datetime < ${date_to}::timestamptz
		ORDER BY ss1.datetime DESC
		LIMIT 1
)
SELECT 'dias de uptime do PostgreSQL' AS texto1,
    CASE
	WHEN round(uptime) = 1 THEN '1 hora'
	WHEN uptime < 1 THEN 'menos de 1 hora'
	WHEN uptime < 24 THEN round(uptime) || ' horas'
	WHEN round(uptime / 24) = 1 THEN '1 dia'
	ELSE round(uptime / 24) || ' dias'
END AS texto2
FROM (
	SELECT EXTRACT(EPOCH FROM age(ss.real_datetime,si.pg_postmaster_start_time)) / 60.0 / 60.0 AS uptime
	FROM sn_instance si INNER JOIN ss USING(snap_id)
) t

