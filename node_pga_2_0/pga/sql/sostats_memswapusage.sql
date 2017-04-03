SELECT
timestamp AS "datetime",
TRUNC(kbswpfree / 1024) AS "mem_swapfree_mb",
TRUNC(kbswpused / 1024) AS "mem_swapused_mb",
TRUNC(kbswpcad / 1024) AS "mem_swapcache_mb"
FROM sn_sysstat_swapusage d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
