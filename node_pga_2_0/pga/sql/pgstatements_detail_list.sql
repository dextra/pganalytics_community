SELECT
    e.log_time AS execution_time,
    ROUND((e.duration / 1000.0)::numeric, 2) AS duration_s,
    /* Maximum of 5 lines of code */
    trim(BOTH E'\n\t\r ' FROM
           substring(e.statement from   1 for 72) || E'\n'
        || substring(e.statement from  73 for 72) || E'\n'
        || substring(e.statement from 145 for 72) || E'\n'
        || substring(e.statement from 217 for 72) || E'\n'
        || substring(e.statement from 289 for 69)
        || CASE
            WHEN length(e.statement) > ((72*5)+69) THEN '...'
            ELSE ''
        END
    ) AS statement_excerpt,
    e.statement
FROM (SELECT duration, regexp_replace(replace(statement, E'\n', ' '), '[ ]+', ' ', 'g') AS statement, server_id, instance_id, database_id, log_time, statement_id FROM sn_statements_executed e) e
WHERE 1=1
    AND server_id = ${server_id}
    AND instance_id = ${instance_id}
    AND (${database_id} = 0 OR database_id = ${database_id})
    AND (log_time >= ${date_from}::timestamptz)
    AND (log_time <= (${date_to}::timestamptz))
    AND e.statement_id = ${statement_id}

