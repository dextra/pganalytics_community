DO
$$
DECLARE
	var_record RECORD;
BEGIN
	FOR var_record IN
		SELECT  'REFRESH MATERIALIZED VIEW CONCURRENTLY ' || pm_customer.schema || '.' || pg_class.relname || ';'  as sql
		FROM pm_customer
		JOIN pg_namespace
			ON pg_namespace.nspname = pm_customer.schema
		JOIN pg_class
			ON 	pg_class.relnamespace = pg_namespace.oid
			AND pg_class.relkind = 'm'
		WHERE pm_customer.is_active
	LOOP
		--RAISE NOTICE 'EXECUTING %', var_record.sql;
		BEGIN
		EXECUTE var_record.sql;
		EXCEPTION WHEN OTHERS THEN
		END;
	END LOOP;
END;
$$;
