SELECT
DISTINCT
spcname AS "tablespace_name",
CASE
        WHEN (spclocation = '' AND spcname = 'pg_default') THEN '$PGDATA'
        WHEN (spclocation IS NULL AND spcname = 'pg_xlog') THEN '$PGDATA/pg_xlog'
        WHEN (spclocation = '' AND spcname = 'pg_global') THEN '$PGDATA/global'
        ELSE spclocation
END AS "spclocation",
(last_value(spcsize) OVER w) / 1024 / 1024 AS "ovw_size_mb",
((last_value(spcsize) OVER w) - (first_value(spcsize) OVER w)) / 1024 / 1024 ||
' ('|| ROUND(((last_value(spcsize) OVER w) - (first_value(spcsize) OVER w)) / ((first_value(spcsize) OVER w) / 100.00),2) ||' %)' AS "ovw_lag_size_mb"
FROM (SELECT snap_id,spcname,spclocation,NULLIF(spcsize,0) AS spcsize FROM sn_tablespace) st
JOIN sn_stat_snapshot sss ON st.snap_id = sss.snap_id
WHERE sss.snap_type = 'pg_stats_global'
AND sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.instance_id = ${instance_id}
AND sss.datetime >= ${date_from}::timestamptz
AND sss.datetime <= ${date_to}::timestamptz + '1 minute'::interval
WINDOW w AS (PARTITION BY spcname ORDER BY sss.datetime ROWS BETWEEN UNBOUNDED PRECEDING AND UNBOUNDED FOLLOWING)
