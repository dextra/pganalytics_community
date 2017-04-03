SELECT 0 AS database_id, '(Todas as bases)' AS database_name
UNION ALL
SELECT d.database_id, d.name AS database_name
FROM pm_database d
JOIN pm_instance i USING(instance_id)
JOIN pm_server s USING (server_id)
WHERE (${customer_id} = 0 OR s.customer_id =  ${customer_id} )
AND (${server_id} = 0 OR s.server_id = ${server_id})
AND (${instance_id} = 0 OR i.instance_id = ${instance_id} )
ORDER BY database_id
