SELECT
DISTINCT
sss.datetime,
st.spcsize / 1024 / 1024 AS "tablespace_size_mb"
FROM (SELECT snap_id,spcname,spclocation,NULLIF(spcsize,0) AS spcsize FROM sn_tablespace) st
JOIN sn_stat_snapshot sss ON st.snap_id = sss.snap_id
WHERE sss.snap_type = 'pg_stats_global'
AND customer_id = ${customer_id}
AND server_id = ${server_id}
AND instance_id = ${instance_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
AND st.spcname = ${tablespace_name}
ORDER BY sss.datetime

