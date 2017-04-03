SELECT
backup_part AS "Parte",
backup_file AS "Arquivo",
TO_CHAR(backup_begin::timestamp,'DD/MM/YYYY HH24:MI:SS') AS "Início",
TO_CHAR(backup_end::timestamp,'DD/MM/YYYY HH24:MI:SS') AS "Término",
TO_CHAR(date_trunc('second',AGE(backup_end::timestamp,backup_begin::timestamp)),'HH24:MI:SS') AS "Duração",
pg_size_pretty(backup_size::bigint) AS "Tamanho"
FROM crosstab('SELECT snap_id,data_key,data_value FROM sn_data_info ORDER BY snap_id','SELECT DISTINCT data_key FROM sn_data_info ORDER BY data_key') 
AS snd(snap_id bigint,BACKUP_BEGIN text,BACKUP_END text,BACKUP_FILE text, BACKUP_PART text, BACKUP_SIZE text) 
JOIN sn_stat_snapshot ss1 USING(snap_id) 
WHERE ss1.snap_type = 'pg_dump'
        AND (ss1.datname = ${database_name})
        AND (date_trunc('day',backup_begin::timestamp) = date_trunc('day',${backup_begin}::timestamp))
ORDER BY "Parte";
