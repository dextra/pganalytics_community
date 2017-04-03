SELECT  DISTINCT ON (d.timestamp)
	timestamp AS "datetime",
	tps AS "disktps",
	(rd_sec_s * 512) / 1024 / 1024 AS "diskread_mb_sec",
	(wr_sec_s * 512) / 1024 / 1024 AS "diskwrite_mb_sec",
	_util AS "diskutil_perc",
	(avgrq_sz * 512.0) / 1024 / 1024 AS "diskreq_mb_size_avg",
	avgqu_sz AS "diskreq_queue_avg",
	await AS "diskreq_wait_ms"
FROM sn_sysstat_disks d
	INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
WHERE
	${device} % replace(d.dev, '!', '/')
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY d.timestamp,similarity(${device},replace(d.dev, '!', '/')) DESC,d.dev
