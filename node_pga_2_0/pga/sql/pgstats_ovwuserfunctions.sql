WITH snapshot AS (
SELECT
        min(sss.snap_id) min_snap_id,
	max(sss.snap_id) max_snap_id
FROM sn_stat_user_functions sss
INNER JOIN sn_stat_snapshot snap on sss.snap_id = snap.snap_id
WHERE 
	snap.snap_type = 'pg_stats'
	AND snap.datetime >= ${date_from}::timestamptz
	AND snap.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND snap.customer_id = ${customer_id}
        AND snap.server_id = ${server_id}
        -- TODO: It doesn't support more than one instance per server
        AND snap.instance_id = ${instance_id}
)
SELECT
	smin.schemaname,
	smin.funcname,
	smax.calls - smin.calls as calls,
	smax.total_time - smin.total_time as total_time,
	smax.self_time - smin.self_time as self_time
FROM
	snapshot
LEFT JOIN
	sn_stat_user_functions  smin on snapshot.min_snap_id = smin.snap_id
LEFT JOIN
	sn_stat_user_functions  smax on snapshot.max_snap_id = smax.snap_id
ORDER BY
	3 DESC,
	1 ASC,
	2 ASC
