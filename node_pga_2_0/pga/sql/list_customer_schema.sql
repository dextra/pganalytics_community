SELECT customer_id, schema, name_id, name
FROM pganalytics.pm_customer
WHERE schema IS NOT NULL AND schema ~ ${user_info.base}
AND is_active
ORDER BY schema
