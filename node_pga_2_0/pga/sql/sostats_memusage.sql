SELECT
timestamp AS "datetime",
CASE
WHEN kbbuffers IS NULL THEN
	/* On Windows, used doesn't consider kbcached */
	TRUNC(kbmemused / 1024)
ELSE
	TRUNC((kbmemused-(kbbuffers + kbcached)) / 1024)
END AS "mem_used_mb",
TRUNC((COALESCE(kbbuffers, 0) + kbcached) / 1024) AS "mem_buffercache_mb",
TRUNC(kbmemfree / 1024) AS "mem_free_mb",
_commit AS "mem_commit_perc"
FROM sn_sysstat_memusage d
INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE 1=1
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp
