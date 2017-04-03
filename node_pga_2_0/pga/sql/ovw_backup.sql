WITH ss_new AS (
    /* Get the newest backup for each database that is <= date_to */
    SELECT DISTINCT ON (ss1.database_id) ss1.snap_id, ss1.datetime, ss1.real_datetime, ss1.database_id
	FROM sn_stat_snapshot ss1
	WHERE ss1.snap_type = 'pg_dump'
		AND (${customer_id} = 0 OR ss1.customer_id = ${customer_id} )
		AND (${server_id} = 0 OR ss1.server_id = ${server_id} )
		AND (${instance_id} = 0 OR ss1.instance_id = ${instance_id} )
		AND (${database_id} = 0 OR ss1.database_id = ${database_id} )
		AND ss1.datetime <= ${date_to}::timestamptz
	ORDER BY ss1.database_id, ss1.datetime DESC
), ss_first AS (
	/* Get the oldest backup for each database that is >= date_from and was not selected by ss_new */
	SELECT DISTINCT ON (ss1.database_id) ss1.snap_id, ss1.datetime, ss1.real_datetime, ss1.database_id
		FROM sn_stat_snapshot ss1
		WHERE ss1.snap_type = 'pg_dump'
			AND NOT EXISTS(SELECT 1 FROM ss_new WHERE ss_new.snap_id = ss1.snap_id)
			AND (${customer_id} = 0 OR ss1.customer_id = ${customer_id} )
			AND (${server_id} = 0 OR ss1.server_id = ${server_id} )
			AND (${instance_id} = 0 OR ss1.instance_id = ${instance_id} )
			AND (${database_id} = 0 OR ss1.database_id = ${database_id} )
			AND ss1.datetime <= ${date_to}::timestamptz
			AND ss1.datetime >= ${date_from}::timestamptz
	ORDER BY ss1.database_id, ss1.datetime
), ss_prev AS (
	/* For each backup retrieved by ss_new, check if there is one selected by ss_first, if not, selected the immediate backup before ss_new */
	SELECT DISTINCT ON (ss1.database_id) ss1.snap_id, ss1.datetime, ss1.real_datetime, ss1.database_id
	FROM sn_stat_snapshot ss1
		INNER JOIN ss_new ssn ON ss1.database_id = ssn.database_id
	WHERE ss1.snap_type = 'pg_dump'
		AND ss1.datetime < ssn.datetime
		AND NOT EXISTS(SELECT 1 FROM ss_first ssf WHERE ssf.database_id = ssn.database_id)
	ORDER BY ss1.database_id, ss1.datetime DESC
)
SELECT
	unnest(
	array[
		'Banco de dados:',
		bkpinfo->'NEW_DATABASE_NAME',
		'Tamanho do arquivo de backup:',
    	pg_size_pretty((bkpinfo->'NEW_BACKUP_SIZE')::bigint),
		'Data/hora do backup:',
		to_char((bkpinfo->'NEW_DATETIME')::timestamptz, 'YYYY-MM-DD HH24:MI'),
		'Tempo para geração do backup:',
		to_char(date_trunc('second', age((bkpinfo->'NEW_BACKUP_END')::timestamptz, (bkpinfo->'NEW_BACKUP_BEGIN')::timestamptz)), 'HH24:MI:SS')
	]
	) AS "Atual",
	unnest(
	array[
		'',
		bkpinfo->'OLD_DATABASE_NAME',
		'',
    	pg_size_pretty((bkpinfo->'OLD_BACKUP_SIZE')::bigint),
		'',
		to_char((bkpinfo->'OLD_DATETIME')::timestamptz, 'YYYY-MM-DD HH24:MI'),
		'',
		to_char(date_trunc('second', age((bkpinfo->'OLD_BACKUP_END')::timestamptz, (bkpinfo->'OLD_BACKUP_BEGIN')::timestamptz)), 'HH24:MI:SS')
	]
	) AS "Anterior"
FROM (
	SELECT
		hstore(
			/* Keys: */
			array_agg(database_key) || array_agg(datetime_key) || array_agg(data_key),
			/* Values: */
			array_agg(d.name) || array_agg(datetime::text) || array_agg(data_value)
		) AS bkpinfo,
    	max(CASE WHEN data_key = 'NEW_BACKUP_SIZE' THEN data_value::bigint ELSE 0 END) AS bkpsize
	FROM (
		SELECT
			'NEW_DATABASE_NAME' AS database_key, ss_new.database_id,
			'NEW_DATETIME' AS datetime_key, ss_new.real_datetime AS datetime,
			'NEW_' || sdi.data_key AS data_key, sdi.data_value
		FROM sn_data_info sdi
			INNER JOIN ss_new USING(snap_id)
		UNION ALL
		SELECT
			'OLD_DATABASE_NAME' AS database_key, ss_first.database_id,
			'OLD_DATETIME' AS datetime_key, ss_first.real_datetime,
			'OLD_' || sdi.data_key AS data_key, sdi.data_value
		FROM sn_data_info sdi
			INNER JOIN ss_first USING(snap_id)
		UNION ALL
		SELECT
			'OLD_DATABASE_NAME' AS database_key, ss_prev.database_id,
			'OLD_DATETIME' AS datetime_key, ss_prev.real_datetime,
			'OLD_' || sdi.data_key AS data_key, sdi.data_value
		FROM sn_data_info sdi
			INNER JOIN ss_prev USING(snap_id)
	) t1
	LEFT JOIN pm_database d ON d.database_id = t1.database_id
	GROUP BY d.database_id, d.name
    ORDER BY bkpsize DESC
) t2

