SELECT
100 - _idle AS value,
100 AS max
FROM sn_sysstat_cpu d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
AND d.cpu = -1
ORDER BY d.timestamp DESC
LIMIT 1

