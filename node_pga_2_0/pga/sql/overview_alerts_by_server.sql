SELECT
	/*a.alert_id,
	a.alert_url,*/
	format(j.job_msg1,REPLACE(a.alert_item,'|','.'),a.alert_value) AS message,
	CASE a.alert_severity
		WHEN 'ok' THEN 'information'
		ELSE a.alert_severity
	END AS alert_severity,
	a.customer_datetime
FROM alert a JOIN pganalytics.job j ON a.job_id = j.job_id
WHERE 1=1
AND a.customer_datetime BETWEEN ${date_from}::timestamptz AND ${date_to}::timestamptz
AND a.customer_id = ${customer_id}
AND a.server_id = ${server_id}
ORDER BY date_trunc('minute', a.customer_datetime) DESC, j.job_type, a.alert_severity

