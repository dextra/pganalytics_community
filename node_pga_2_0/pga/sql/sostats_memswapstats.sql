SELECT
timestamp AS "datetime",
pswpin_s AS "mem_swapin_sec",
pswpout_s AS "mem_swapout_sec"
FROM sn_sysstat_swapstats d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
