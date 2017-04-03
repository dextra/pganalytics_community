WITH last_snapshot AS (
SELECT
        sss.snap_id
FROM sn_stat_snapshot sss
WHERE sss.snap_type = 'pg_stats_global'
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	-- TODO: It doesn't support more than one instance per server
	AND sss.instance_id = ${instance_id}
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY sss.datetime DESC LIMIT 1
)
SELECT
        s.name AS "parameter",
        util.set_unit_setting(s.name,s.setting) AS "setting"
FROM sn_settings s
        JOIN last_snapshot ls ON s.snap_id = ls.snap_id
