SELECT  
	/*alert_url,*/
	format(job_msg1,REPLACE(alert_item,'|','.'),alert_value) AS message,
	CASE alert_severity
		WHEN 'ok' THEN 'information'
		ELSE alert_severity END AS alert_severity,
	customer_datetime,
	alert_time::timestamptz
FROM pganalytics.alert al
JOIN pganalytics.job j ON al.job_id=j.job_id 
WHERE j.job_id = (SELECT job_id FROM alert WHERE alert_id = ${alert_id})
AND customer_id = ${customer_id}
AND server_id = ${server_id}
ORDER BY date_trunc('minute',alert_time) DESC, customer_datetime DESC, job_type, alert_severity
