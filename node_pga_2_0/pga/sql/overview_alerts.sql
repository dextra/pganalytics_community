SELECT srv.name AS server_name,
COUNT(CASE WHEN a.alert_severity = 'critical' THEN 1 END) AS critical,
COUNT(CASE WHEN a.alert_severity = 'warning' THEN 1 END) AS warning,
COUNT(CASE WHEN a.alert_severity = 'ok' THEN 1 END) AS information
FROM pm_server srv LEFT JOIN alert a ON a.server_id = srv.server_id
WHERE a.customer_datetime BETWEEN ${date_from}::timestamptz AND ${date_to}::timestamptz
GROUP BY srv.name
ORDER BY critical, warning, information, server_name
