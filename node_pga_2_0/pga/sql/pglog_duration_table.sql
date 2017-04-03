/*
INSERT INTO pga_config.widget(key, section, type, extra_param, priority)
VALUES('pglog_duration_table', 'sql', 'dataTable', E'{\n\t"subType": "sql"\n}', 0);
*/

SELECT
ROUND((SUM(s.duration) / 1000)::numeric,2) AS "Tempo Total(s)",
ROUND((MIN(s.duration) / 1000)::numeric,2) AS "Tempo Mínimo(s)",
ROUND((MAX(s.duration) / 1000)::numeric,2) AS "Tempo Máximo(s)",
ROUND((AVG(s.duration) / 1000)::numeric,2) AS "Tempo Médio(s)",
count(*) AS "Num. Execuções",
substring(s.statement_norm from 1 for 72) || E'\n' || substring(s.statement_norm from 71 for 69)
    || CASE
        WHEN length(s.statement_norm) > (72+69) THEN '...'
        ELSE ''
    END AS "Comando SQL"
,MIN(s.statement) AS "Exemplo"
FROM (
    /* hack to normalize IN (...) */
    SELECT
        regexp_replace(statement_norm, 'IN[\s]*\([^\)]*\)', 'IN (...)', 'gi') AS statement_norm,
        duration, statement, server_id, instance_id, database_id, log_time
    FROM vw_sn_pglog_statements
) s
WHERE 1=1
AND server_id = ${server_id}
AND instance_id = ${instance_id}
AND (${database_id} = 0 OR database_id = ${database_id})
AND (log_time >= ${date_from}::timestamptz)
AND (log_time <= (${date_to}::timestamptz))
GROUP BY statement_norm
ORDER BY count(*) DESC
LIMIT 100;
