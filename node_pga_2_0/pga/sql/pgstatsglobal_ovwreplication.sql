WITH snapshot AS (
SELECT
        min(sss.snap_id) min_snap_id,
	max(sss.snap_id) max_snap_id
FROM sn_stat_replication sss
INNER JOIN sn_stat_snapshot snap on sss.snap_id = snap.snap_id
WHERE 
	snap.snap_type = 'pg_stats_global'
	AND snap.datetime >= ${date_from}::timestamptz
	AND snap.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND snap.customer_id = ${customer_id}
        AND snap.server_id = ${server_id}
        -- TODO: It doesn't support more than one instance per server
        AND snap.instance_id = ${instance_id}
)
SELECT
	smin.state as initial_state,
        smax.state as last_state,
	smin.sent_location as initial_sent_location,
	smax.sent_location as last_sent_location,
	smin.write_location  as initial_write_location,
	smax.write_location as last_write_location,
 	smin.flush_location as initial_flush_location,
	smax.flush_location as last_flush_location,
 	smin.replay_location as initial_replay_location,
	smax.replay_location as last_replay_location,
	smax.sent_location_diff - smin.sent_location_diff as sent_location_diff,
	smax.write_location_diff - smin.write_location_diff as write_location_diff,
	smax.flush_location_diff - smin.flush_location_diff as flush_location_diff,
	smax.replay_location_diff - smin.replay_location_diff as replay_location_diff
FROM
	snapshot
LEFT JOIN
	sn_stat_replication  smin on snapshot.min_snap_id = smin.snap_id
LEFT JOIN
	sn_stat_replication  smax on snapshot.max_snap_id = smax.snap_id
