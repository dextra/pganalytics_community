SELECT DISTINCT ON(sdu.fsdevice)
	sdu.fsdevice AS device,
	sdu.fstype,
	sdu.size / 1024 AS size_mb,
	sdu.used / 1024 AS used_mb,
	sdu.available / 1024 AS available_mb,
	sdu.usage AS usage_perc,
	sdu.mountpoint
FROM sn_disk_usage sdu
	INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = sdu.snap_id)
WHERE NOT sdu.fstype LIKE ANY(array['tmpfs', 'devtmpfs', 'cifs', 'iso%','CDROM%'])
	AND sss.customer_id = ${customer_id}
	AND sss.server_id = ${server_id}
	AND sss.datetime >= ${date_from}::timestamptz
	AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
ORDER BY sdu.fsdevice, sss.datetime DESC

