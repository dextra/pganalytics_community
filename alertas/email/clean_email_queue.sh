#!/bin/bash

psql -U pganalytics <<EOF
BEGIN;
UPDATE alert SET alert_sent_time = 'infinity' 
	WHERE alert_id IN (SELECT alert_id FROM alert 
			   JOIN pm_customer USING(customer_id) 
			   JOIN job USING(job_id) 
			   WHERE alert_sent_time IS NULL 
			   AND job_type = 'alarm' 
			   ORDER BY customer_datetime DESC);
COMMIT;
EOF
