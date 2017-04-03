WITH snapshot AS (
SELECT
	datid,
        min(sss.snap_id) min_snap_id,
	max(sss.snap_id) max_snap_id
FROM sn_stat_database_conflicts sss
INNER JOIN sn_stat_snapshot snap on sss.snap_id = snap.snap_id
WHERE 
	snap.snap_type = 'pg_stats'
	AND snap.datetime >= ${date_from}::timestamptz
	AND snap.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND snap.customer_id = ${customer_id}
        AND snap.server_id = ${server_id}
        -- TODO: It doesn't support more than one instance per server
        AND snap.instance_id = ${instance_id}
GROUP BY datid
)
SELECT
	smin.datname,
	smax.confl_tablespace - smin.confl_tablespace as confl_tablespace,
	smax.confl_lock - smin.confl_lock as confl_lock,
	smax.confl_snapshot - smin.confl_snapshot as confl_snapshot,
	smax.confl_bufferpin - smin.confl_bufferpin as confl_bufferpin,
	smax.confl_deadlock - smin.confl_deadlock as confl_deadlock
FROM
	snapshot
LEFT JOIN
	sn_stat_database_conflicts  smin on snapshot.min_snap_id = smin.snap_id
LEFT JOIN
	sn_stat_database_conflicts  smax on snapshot.max_snap_id = smax.snap_id
ORDER BY
	3 DESC,
	1 ASC,
	2 ASC
