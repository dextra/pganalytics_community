select customer_id, schema 
from pganalytics.pm_customer 
where schema is not null 
and name_id = ${name_id}
order by schema