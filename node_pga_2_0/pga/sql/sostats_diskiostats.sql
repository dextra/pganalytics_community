SELECT
timestamp AS "datetime",
tps AS "diskiotps",
rtps AS "diskioreq_read_sec",
wtps AS "diskioreq_write_sec",
(bread_s * 512) / 1024 / 1024 AS "diskioblocks_mb_read_sec",
(bwrtn_s * 512) / 1024 / 1024 AS "diskioblocks_mb_write_sec"
FROM sn_sysstat_io d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
