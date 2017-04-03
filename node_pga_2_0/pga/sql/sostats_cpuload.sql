SELECT
timestamp AS "datetime",
ldavg_1 AS "cpu_load1_avg",
ldavg_5 AS "cpu_load5_avg",
ldavg_15 AS "cpu_load15_avg",
runq_sz AS "cpu_runqueue_count",
plist_sz AS "cpu_tasklist_count",
blocked AS "cpu_taskblocked_count"
FROM sn_sysstat_loadqueue d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
