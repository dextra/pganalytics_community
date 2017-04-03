SELECT diagnostic AS "Diagnóstico",recomendation AS "Recomendação",
TO_CHAR((TRIM(REPLACE(split_part(d.expire::text,',',1),'[',''),'"'))::timestamp,'DD/MM/YYYY') AS "Data",
priority AS "Prioridade" 
FROM sn_diagnostic d 
JOIN sn_stat_snapshot ss1 USING (snap_id)
WHERE ss1.snap_type = 'diagnostic'
AND (${customer_id} = 0 OR ss1.customer_id = ${customer_id} )
AND (${server_id} = 0 OR ss1.server_id = ${server_id} )
AND (${instance_id} = 0 OR ss1.instance_id = ${instance_id} )
AND (${database_id} = 0 OR ss1.database_id = ${database_id} )
AND d.expire && tstzrange(${date_from}::timestamptz, ${date_to}::timestamptz, '[]')
ORDER BY (TRIM(REPLACE(split_part(d.expire::text,',',1),'[',''),'"'))::timestamp DESC;
