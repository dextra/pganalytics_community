SELECT TO_CHAR(log_time,'DD/MM/YYYY HH24:MI:SS') AS "Data",error_severity AS "Tipo de mensagem",message AS "Mensagem" FROM sn_pglog
WHERE database_name = 'raddb'
AND (log_time >= ${date_from}::timestamptz)
AND (log_time <= (${date_to}::timestamptz + '1 hour'::interval))
ORDER BY log_time DESC
LIMIT 100;
