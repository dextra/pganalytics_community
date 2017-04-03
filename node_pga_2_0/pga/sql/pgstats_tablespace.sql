SELECT DISTINCT ON (server_name,instance_name,spcname)
        server_name AS "server_name",
        instance_name AS "instance_name",
        spcname AS "tablespace_name",
        spclocation AS "tablespace_location",
        spcsize / 1024 / 1024 AS "tablespace_size_mb",
        (spcsize - lag) / 1024 ||' ('||ROUND((spcsize - lag) / (lag / 100)::numeric,2)||'%)' AS "lag_tablespace_size_kb"
FROM
        (SELECT
                sss.datetime,
                srv.name AS "server_name",
                ins.name AS "instance_name",
                spcname,
                spcsize,
                lag(spcsize) OVER w AS lag,
                spclocation
        FROM (SELECT snap_id,spcname,spclocation,NULLIF(spcsize,0) AS spcsize FROM sn_tablespace) st
        JOIN vw_last_two_snapshots sss ON st.snap_id = sss.snap_id
        JOIN pm_server srv ON sss.server_id=srv.server_id
        JOIN pm_instance ins ON sss.instance_id=ins.instance_id
        WHERE sss.snap_type = 'pg_stats_global'
        AND customer_id = ${customer_id}
        AND server_id = ${server_id}
        AND instance_id = ${instance_id}
	WINDOW w AS (PARTITION BY spcname ORDER BY st.snap_id)
        ) AS tbl
WHERE lag IS NOT NULL
ORDER BY server_name,instance_name,spcname,datetime
