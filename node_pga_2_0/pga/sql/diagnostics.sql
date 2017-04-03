SELECT sss.datetime, sd.diagnostic, sd.recomendation, sd.priority, sd.autor
FROM sn_stat_snapshot sss INNER JOIN sn_diagnostic sd ON sss.snap_id = sd.snap_id
WHERE sss.customer_id = ${customer_id}
AND sss.server_id = ${server_id}
AND sss.instance_id = ${instance_id}
AND (${database_id} = 0 OR sss.database_id = ${database_id} )
AND expire && tstzrange(${date_from}::timestamptz, ${date_to}::timestamptz + '1 minute'::interval, '[]')
ORDER BY sss.datetime DESC

