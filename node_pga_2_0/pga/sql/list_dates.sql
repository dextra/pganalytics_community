SELECT 
    min(date_trunc('minute', ss.datetime)),
    max(date_trunc('minute', ss.datetime)) - interval '8h',
    max(date_trunc('minute', ss.datetime))
FROM sn_stat_snapshot ss
WHERE ss.snap_type IN ('pg_stats','sysstat','df')
    AND (${customer_id} = 0 OR ss.customer_id =  ${customer_id} )
    AND (${server_id} = 0 OR ss.server_id = ${server_id})
    AND (${instance_id} = 0 OR ss.instance_id = ${instance_id} )
