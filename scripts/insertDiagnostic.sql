\pset footer off

SELECT customer_id AS customer FROM pm_customer_get_current() \gset 

SELECT server_id,name FROM pm_server;
\prompt 'Choose the server:  ' server

SELECT instance_id, name FROM pm_instance WHERE server_id = :server ;
\prompt 'Choose the instance:  ' instance

SELECT database_id,name FROM pm_database WHERE instance_id = COALESCE(nullif(:'instance','')::integer,0);
\prompt 'Choose the database:  ' database

\prompt 'Diagnostic:  ' diagnostic
\prompt 'Recomendation:   ' recomendation
\prompt 'Priority [Alta, Média, Baixa]:  ' priority
\prompt 'Who am I?:  ' autor
\prompt 'Validity [today,infinity]:   ' expire

BEGIN;

INSERT INTO sn_stat_snapshot 
(snap_id, customer_id, server_id, instance_id, database_id, datetime, snap_type, real_datetime) 
VALUES
(default, :customer, :server, nullif(:'instance', '')::integer, nullif(:'database', '')::integer, now(), 'diagnostic',now());

INSERT INTO sn_diagnostic
(diagnostic,recomendation,priority,is_automatic,autor,expire) 
VALUES
(nullif(:'diagnostic', ''),
nullif(:'recomendation', ''),
nullif(:'priority', ''),
false,
nullif(:'autor', ''),
COALESCE(nullif(:'expire', '')::text,'[today,infinity]')::tstzrange);

COMMIT;
