WITH snaps AS (
	SELECT sss.server_id, sss.instance_id, sss.database_id, sss.datetime, sss.snap_id
	FROM sn_stat_snapshot sss
	WHERE (sss.server_id = ${server_id} )
		AND (sss.instance_id = ${instance_id} )
		AND (0 = 0 OR sss.database_id = ${database_id} )
		AND (sss.datetime >= ${date_from}::timestamptz)
		AND (sss.datetime <= (${date_to}::timestamptz + '1 hour'::interval))
		OR sss.snap_id IN (
			/* Get first snapshot before date_from */
			SELECT DISTINCT ON(s2.database_id) s2.snap_id
			FROM sn_stat_snapshot s2
			WHERE (s2.server_id = ${server_id} )
				AND (s2.instance_id = ${instance_id} )
				AND (0 = 0 OR s2.database_id = ${database_id} )
				AND (s2.datetime < ${date_from}::timestamptz)
			ORDER BY s2.database_id, s2.datetime DESC
		)
)
SELECT DISTINCT ON (datetime)
datetime,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	bw.checkpoints_timed - (lag(bw.checkpoints_timed) OVER w)
ELSE
	NULL
END AS checkpoint_timed,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	bw.checkpoints_req - (lag(bw.checkpoints_req) OVER w)
ELSE
	NULL
END AS checkpoint_req,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	bw.checkpoint_write_time - (lag(bw.checkpoint_write_time) OVER w)
ELSE
	NULL
END AS checkpoint_write_time_ms,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	bw.checkpoint_sync_time - (lag(bw.checkpoint_sync_time) OVER w)
ELSE
	NULL
END AS checkpoint_sync_time_ms,
/* Buffers */
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	ROUND(((bw.buffers_checkpoint - (lag(bw.buffers_checkpoint) OVER w)) * 8192.0) / 1024.0 / 1024.0, 2)
ELSE
	NULL
END AS buffers_checkpoint_mb,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	ROUND(((bw.buffers_clean - (lag(bw.buffers_clean) OVER w)) * 8192.0) / 1024.0 / 1024.0, 2)
ELSE
	NULL
END AS buffers_clean_mb,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	ROUND(((bw.buffers_backend - (lag(bw.buffers_backend) OVER w)) * 8192.0) / 1024.0 / 1024.0, 2)
ELSE
	NULL
END AS buffers_backend_mb,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	ROUND(((bw.buffers_alloc - (lag(bw.buffers_alloc) OVER w)) * 8192.0) / 1024.0 / 1024.0, 2)
ELSE
	NULL
END AS buffers_alloc_mb,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	ROUND(((bw.buffers_backend_fsync - (lag(bw.buffers_backend_fsync) OVER w)) * 8192.0) / 1024.0 / 1024.0, 2)
ELSE
	NULL
END AS buffers_backend_fsync_mb,
CASE WHEN bw.stats_reset = (lag(bw.stats_reset) OVER w) THEN
	bw.maxwritten_clean - (lag(bw.maxwritten_clean) OVER w)
ELSE
	NULL
END AS maxwritten_clean
FROM sn_stat_bgwriter bw
INNER JOIN snaps ON (snaps.snap_id = bw.snap_id)
WINDOW w AS (ORDER BY datetime)
ORDER BY datetime
OFFSET 1;

