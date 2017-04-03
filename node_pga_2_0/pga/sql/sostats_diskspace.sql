WITH ss AS (
    SELECT DISTINCT ss1.datetime,ss1.customer_id,ss1.server_id,ts.fsdevice, SUM(ts.spcsize) AS spcsize
        FROM sn_stat_snapshot ss1
        JOIN sn_tablespace ts ON ts.snap_id = ss1.snap_id
        WHERE ss1.snap_type = 'pg_stats'
        AND ss1.customer_id = ${customer_id}
        AND ss1.server_id = ${server_id}
        AND ss1.datetime >= ${date_from}::timestamptz
        AND ss1.datetime <= ${date_to}::timestamptz + '1 minute'::interval
	AND (${device} LIKE '%'||replace(ts.fsdevice, '!', '/')||'%')
	GROUP BY ss1.datetime,ss1.customer_id,ss1.server_id,ts.fsdevice,ss1.snap_id
	ORDER BY ss1.datetime
), df AS (
    SELECT ss2.datetime,df.usage, df.used, df.available, COALESCE((ss.spcsize / 1024),0) AS pg_size
        FROM sn_stat_snapshot ss2
        JOIN sn_disk_usage df ON df.snap_id = ss2.snap_id 
        LEFT JOIN ss ON (ss2.datetime,ss2.customer_id,ss2.server_id,df.fsdevice) = (ss.datetime,ss.customer_id,ss.server_id,ss.fsdevice)
        WHERE ss2.snap_type = 'df'
	AND ss2.customer_id = ${customer_id}
        AND ss2.server_id = ${server_id}
        AND ss2.datetime >= ${date_from}::timestamptz
        AND ss2.datetime <= ${date_to}::timestamptz + '1 minute'::interval
        AND NOT df.fstype LIKE ANY(array['tmpfs', 'devtmpfs', 'cifs', 'iso%','CDROM%'])
	AND (${device} LIKE '%'||replace(df.fsdevice, '!', '/')||'%')
)
SELECT
datetime,
round(pg_size::numeric / 1024, 2) AS diskspace_postgresql,
round((used - pg_size)::numeric / 1024, 2) AS diskspace_others,
round((available / 1024)::numeric, 2) AS diskspace_free
FROM df
