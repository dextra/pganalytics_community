SELECT
timestamp AS "datetime",
_user AS "cpu_user_perc",
_system AS "cpu_system_perc",
_nice AS "cpu_nice_perc",
_iowait AS "cpu_iowait_perc",
_steal AS "cpu_steal_perc",
_idle AS "cpu_idle_perc"
FROM sn_sysstat_cpu d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
AND d.cpu = -1
ORDER BY d.timestamp;

