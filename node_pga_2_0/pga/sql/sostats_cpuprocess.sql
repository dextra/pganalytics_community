SELECT
timestamp AS "datetime",
proc_s AS "cpu_taskcreated_sec",
cswch_s AS "cpu_contextswt_sec"
FROM sn_sysstat_tasks d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
