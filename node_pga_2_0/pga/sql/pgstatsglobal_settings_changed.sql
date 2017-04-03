WITH changes AS (
SELECT
        LAG(sss.datetime) OVER w AS old_datetime,
        sss.datetime,
        LAG(s.setting) OVER w AS old_setting,
        s.name AS "parameter",
        s.setting AS "setting",
        si.pg_postmaster_start_time
FROM sn_settings s
        JOIN sn_stat_snapshot sss ON s.snap_id = sss.snap_id
        JOIN sn_instance si ON s.snap_id = si.snap_id
WHERE sss.snap_type = 'pg_stats_global'
        AND sss.customer_id = ${customer_id}
        AND sss.server_id = ${server_id}
        -- TODO: It doesn't support more than one instance per server
        AND sss.instance_id = ${instance_id}
        AND sss.datetime >= ${date_from}::timestamptz
        AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
WINDOW w AS (PARTITION BY s.name ORDER BY sss.datetime,s.name)
ORDER BY sss.datetime
)
SELECT
        TO_CHAR(w.datetime,'DD/MM/YYYY HH24:MI:00') AS change_time,
        w.parameter,
        util.set_unit_setting(w.parameter,w.old_setting) AS from_setting,
        util.set_unit_setting(w.parameter,w.setting) AS to_setting,
        TO_CHAR(w.pg_postmaster_start_time,'DD/MM/YYYY HH24:MI:SS') AS postgres_start_time
FROM changes w
        JOIN pg_settings pgs ON pgs.name=w.parameter
WHERE w.setting <> w.old_setting
