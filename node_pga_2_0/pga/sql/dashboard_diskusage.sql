SELECT avg(value) AS value, 100 AS max
FROM (
	SELECT  DISTINCT ON (d.dev)
		_util AS value
	FROM sn_sysstat_disks d
		INNER JOIN sn_stat_snapshot sss ON (sss.snap_id = d.snap_id)
	WHERE
		sss.customer_id = ${customer_id}
		AND sss.server_id = ${server_id}
		AND sss.datetime >= ${date_from}::timestamptz
		AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	ORDER BY d.dev, d.timestamp DESC
) t

