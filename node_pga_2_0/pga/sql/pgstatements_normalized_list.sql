SELECT
    s.statement_id AS statement_id,
    ROUND((SUM(e.duration) / 1000.0)::numeric,2) AS total_duration_s,
    ROUND((MIN(e.duration) / 1000.0)::numeric,2) AS min_duration_s,
    ROUND((MAX(e.duration) / 1000.0)::numeric,2) AS max_duration_s,
    ROUND((AVG(e.duration) / 1000.0)::numeric,2) AS avg_duration_s,
    count(*) AS exec_count,
    substring(s.statement_norm from 1 for 72) || E'\n' || substring(s.statement_norm from 71 for 69)
        || CASE
            WHEN length(s.statement_norm) > (72+69) THEN '...'
            ELSE ''
        END AS statement_norm
FROM sn_statements_executed e INNER JOIN sn_statements s ON s.statement_id = e.statement_id
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (${database_id} = 0 OR database_id = ${database_id})
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
GROUP BY s.statement_id
