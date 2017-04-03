INSERT INTO job (
job_nameid,
job_description,
job_query,
job_msg1,
job_msg2,
job_warn_thr,
job_crit_thr,
job_deep,
job_enabled,
job_alert_limit,
job_result_type,
job_bad_value,
job_type)
VALUES (
'diagnostic_received',							 -- Name without spaces
'Novos diagnósticos e recomendações',					 -- Job full name
$$SELECT
        'Diagnóstico: '||diagnostic AS item,
        'Recomendação: '||recomendation AS value,
        vw.customer_id,
        vw.server_id,
        0 AS instance_id,
        0 AS database_id 
FROM %1$I.sn_diagnostic t 
JOIN %1$I.vw_sn_last_snapshots vw ON t.snap_id=vw.last_snap_id
$$,									-- Query to check this job. it must return the attributes below:
									-- item (text), value (same as job_result_type field), 
									-- customer_id, server_id, instance_id, database_id 
									-- To specify schemas, you should use %1$I mask.
'Você recebeu novos diagnósticos e recomendações',			-- Message to detail the alert. You can use %1$s and %2$s (item,value)
'',					 				-- Another message, maybe you might want to write advices to the user
									-- You can use %1$s and %2$s (item,value)
'100',								 	-- Warning threshold. The type is always text, but format depends from job_result_type		 
'200',								 	-- Critical threshold. Same as warning
'server_id',								-- Job level (customer_id,server_id,instance_id,database_id)
't',									-- Boolean to enable or disable the job
3,									-- Defines how many times the email will be sent
'integer',								-- Real data type for "value" column 
'>',									-- The bad value is the largest or the smallest? ('>','<')
'information'								-- Job type can be information or alarm for while
)
;
