SELECT i.instance_id, i.port, i.name||':'||i.port AS instance_name
FROM pm_instance i
JOIN pm_server s USING (server_id)
WHERE (${customer_id} = 0 OR s.customer_id = ${customer_id} )
AND (${server_id} = 0 OR s.server_id = ${server_id} )
