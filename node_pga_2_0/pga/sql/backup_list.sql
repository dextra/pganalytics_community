SELECT * FROM (
SELECT 
datname AS database_name,
backup_file,
backup_begin,
backup_end,
duration || COALESCE(' ('||(duration - lag(duration) OVER w)||')','') AS duration,
pg_size_pretty(backup_size::bigint) || COALESCE(' ('||pg_size_pretty((backup_size - lag(backup_size) OVER w)::bigint)||')','') AS backup_size ,
parts || COALESCE(' ('||(parts - lag(parts) OVER w)||')','') AS backup_parts
FROM (
        SELECT
        datname,
        replace(backup_file,backup_part, '*') AS backup_file,
        backup_begin AS backup_begin,
        backup_end AS backup_end,
        duration::interval AS duration,
        backup_size AS backup_size,
        CASE WHEN customer_id = 24 THEN count(*) OVER k ELSE 0 END AS parts
        FROM
        (SELECT
	ss1.customer_id,
        ss1.datname,
        backup_begin::timestamptz AS backup_begin,
        backup_end::timestamptz AS backup_end,
        TO_CHAR(date_trunc('second',AGE(backup_end::timestamp,backup_begin::timestamp)),'HH24:MI:SS') AS duration,
        backup_file AS backup_file,
        backup_part AS backup_part,
        backup_size::bigint
        FROM crosstab('SELECT snap_id,data_key,data_value FROM sn_data_info ORDER BY snap_id','SELECT DISTINCT data_key FROM sn_data_info ORDER BY data_key')
        AS snd(snap_id bigint,BACKUP_BEGIN text,BACKUP_END text,BACKUP_FILE text, BACKUP_PART text, BACKUP_SIZE text)
        JOIN sn_stat_snapshot ss1 USING(snap_id)
        WHERE ss1.snap_type = 'pg_dump'
            AND ss1.customer_id = ${customer_id}
            AND (${server_id} = 0 OR ss1.server_id = ${server_id} )
	    AND (${instance_id} = 0 OR ss1.instance_id = ${instance_id} )
	    AND (${database_id} = 0 OR ss1.database_id = ${database_id} )
        ORDER BY backup_begin::timestamp DESC) AS backup_info
        WINDOW k AS (PARTITION BY replace(backup_file,backup_part, '*'))
        ORDER BY backup_end DESC
) AS sn_last_backup
WINDOW w AS (PARTITION BY datname ORDER BY date_trunc('day',backup_begin::timestamp) )) AS calc 
WHERE backup_parts IS NOT NULL 
