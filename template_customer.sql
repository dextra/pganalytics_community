--
-- PostgreSQL database dump
--

SET statement_timeout = 0;
SET lock_timeout = 0;
SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;
SET check_function_bodies = false;
SET client_min_messages = warning;

--
-- Name: template_customer; Type: SCHEMA; Schema: -; Owner: pganalytics
--

CREATE SCHEMA template_customer;


ALTER SCHEMA template_customer OWNER TO pganalytics;

SET search_path = template_customer, pg_catalog;

--
-- Name: snap_type_enum; Type: TYPE; Schema: template_customer; Owner: pganalytics
--

CREATE TYPE snap_type_enum AS ENUM (
    'pg_stats',
    'pg_stats_global',
    'sysstat',
    'df',
    'pg_dump',
    'diagnostic',
    'pg_log'
);


ALTER TYPE snap_type_enum OWNER TO pganalytics;

--
-- Name: log_error(text, text, text, text, text, integer); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION log_error(customer text, action text, errcode text, message text, source text, line integer) RETURNS void
    LANGUAGE sql
    AS $_$
	INSERT INTO pganalytics.pga_error_log(customer_id, action, errcode, message, source, line)
	VALUES((SELECT customer_id FROM pganalytics.pm_customer WHERE name_id = $1), $2, $3, $4, $5, $6);
$_$;


ALTER FUNCTION template_customer.log_error(customer text, action text, errcode text, message text, source text, line integer) OWNER TO pganalytics;

--
-- Name: pm_customer_get_current(); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION pm_customer_get_current() RETURNS pganalytics.pm_customer
    LANGUAGE sql SECURITY DEFINER
    AS $$
	SELECT * FROM pganalytics.pm_customer WHERE schema = current_schema();
$$;


ALTER FUNCTION template_customer.pm_customer_get_current() OWNER TO pganalytics;

--
-- Name: pm_database_get_id(name, integer); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION pm_database_get_id(p_datname name, p_instance_id integer) RETURNS integer
    LANGUAGE plpgsql STRICT SECURITY DEFINER
    AS $$
DECLARE
	v_id integer;
	i integer;
BEGIN
	FOR i IN 1..100 LOOP -- don't try forever, it is OK to give up sometime
		SELECT d.database_id INTO v_id
		FROM pm_database d
		WHERE d.instance_id = p_instance_id AND d.name = p_datname;
		IF (FOUND) THEN
			RETURN v_id;
		END IF;
		BEGIN
			INSERT INTO pm_database(instance_id, name)
			VALUES (p_instance_id, p_datname)
			RETURNING database_id INTO v_id;
			RETURN v_id;
		EXCEPTION
			WHEN unique_violation THEN
				NULL;
		END;
		-- Sleep a little bit at each try, just to make some time for concurrent transactions and avoid many locks
		PERFORM pg_sleep(0.2);
	END LOOP;
	RAISE 'Could not get database_id' USING HINT = 'Check if any new UNIQUE constraint broke this code!';
END;
$$;


ALTER FUNCTION template_customer.pm_database_get_id(p_datname name, p_instance_id integer) OWNER TO pganalytics;

--
-- Name: sn_import_snapshot(snap_type_enum, text, text, timestamp with time zone, timestamp with time zone, text, text, text); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
	v_current_customer_id integer;
	v_server_id integer;
	v_instance_id integer;
	v_database_id integer;
BEGIN
	/* Get and validate the customer_id */
	SELECT c.customer_id INTO v_current_customer_id
	FROM pm_customer_get_current() c
	WHERE c.name_id = sn_import_snapshot.customer_name;
	IF (v_current_customer_id IS NULL) THEN
		RAISE 'Invalid customer name';
	END IF;
	/* Get and validate the server_id */
	SELECT s.server_id INTO v_server_id
	FROM pm_server s
	WHERE s.name = sn_import_snapshot.server_name;
	IF (v_server_id IS NULL) THEN
		RAISE 'Invalid server name';
	END IF;
	/* Get the instance_id, no need to validate, instance can be NULL */
	SELECT i.instance_id INTO v_instance_id
	FROM pm_instance i
	WHERE i.name = sn_import_snapshot.instance_name AND i.server_id = v_server_id;
	IF (v_instance_id IS NOT NULL AND sn_import_snapshot.datname IS NOT NULL) THEN
		v_database_id := pm_database_get_id(sn_import_snapshot.datname, v_instance_id);
	END IF;
	/* Insert the data into sn_stat_snapshot */
	INSERT INTO sn_stat_snapshot(snap_id, customer_id, server_id, instance_id, datetime, snap_type, datname, database_id, snap_hash, real_datetime)
	VALUES(default, v_current_customer_id, v_server_id, v_instance_id, sn_import_snapshot.datetime, sn_import_snapshot.snap_type, sn_import_snapshot.datname, v_database_id, snap_hash, sn_import_snapshot.real_datetime);
END;
$$;


ALTER FUNCTION template_customer.sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) OWNER TO pganalytics;

--
-- Name: sn_stat_snapshot_finish(integer); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_stat_snapshot_finish(p_snap_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
	v_snap_type sn_stat_snapshot.snap_type%TYPE;
BEGIN
	SELECT s.snap_type INTO v_snap_type FROM sn_stat_snapshot s WHERE s.snap_id = p_snap_id;
	CASE v_snap_type
	WHEN 'pg_log' THEN
		DROP TABLE IF EXISTS pg_temp.tmp_statements;
		-- RAISE NOTICE '% - Parsing logs for statements', clock_timestamp();
		CREATE TEMP TABLE tmp_statements ON COMMIT DROP AS
			SELECT
				sss.snap_id,
				sss.server_id,
				sss.instance_id,
				d.database_id,
				l.log_time,
				l.pid,
				l.session_line_num,
				l.application_name,
				l.user_name,
				l.database_name,
				l.remote_host,
				substring(l.message FROM '^duration: (\d+\.\d+) ms')::double precision AS duration,
				substring(l.message FROM '^duration: \d+\.\d+ ms  (?:statement|execute|parse|bind)[^:]*: (.*)') AS statement,
				substring(l.normalized FROM '^duration: \? ms (?:statement|execute|parse|bind)[^:]*: (.*)') AS statement_norm,
				decode(md5(substring(l.normalized FROM '^duration: \? ms (?:statement|execute|parse|bind)[^:]*: (.*)')), 'hex') AS statement_md5
			FROM sn_pglog l
				JOIN sn_stat_snapshot sss ON sss.snap_id = l.snap_id
				LEFT JOIN pm_database d ON d.instance_id = sss.instance_id AND d.name = l.database_name
			WHERE
				l.normalized ~ '^duration: .* ms (statement|execute|parse|bind).*:'
				AND l.error_severity = 'LOG'
				AND l.normalized !~ '(COMMIT|EXPLAIN|COPY)'
				AND l.snap_id = p_snap_id;
		-- RAISE NOTICE 'Temp logs table size: %', pg_size_pretty(pg_table_size('tmp_statements'::regclass));
		-- RAISE NOTICE '% - Indexing it...', clock_timestamp();
		/* TODO: Check if it is really faster with the index in place */
		CREATE INDEX ON tmp_statements(statement_md5);
		-- RAISE NOTICE '% - Analyzing it...', clock_timestamp();
		ANALYZE tmp_statements;
		/* Guarantee no one is writing to this table, so the UPSERT is safe */
		-- LOCK sn_statements IN ROW EXCLUSIVE MODE;
		-- RAISE NOTICE '% - Inserting into sn_statements...', clock_timestamp();
		INSERT INTO sn_statements(statement_md5, statement_norm)
		SELECT
			s1.statement_md5, s2.statement_norm
			FROM (
				SELECT DISTINCT si1.statement_md5
				FROM tmp_statements si1
			) s1, LATERAL(
				SELECT DISTINCT si2.statement_norm
				FROM tmp_statements si2
				WHERE si2.statement_md5 = s1.statement_md5
			) s2
		WHERE NOT EXISTS(SELECT 1 FROM sn_statements s WHERE s.statement_md5 = s1.statement_md5 AND s.statement_norm = s2.statement_norm);
		-- RAISE NOTICE '% - Finaly, inserting into sn_statements_executed...', clock_timestamp();
		INSERT INTO sn_statements_executed(
			snap_id,
			server_id,
			instance_id,
			database_id,
			log_time,
			pid,
			session_line_num,
			application_name,
			user_name,
			database_name,
			remote_host,
			duration,
			statement_id,
			statement
		)
		SELECT
			t.snap_id,
			t.server_id,
			t.instance_id,
			t.database_id,
			t.log_time,
			t.pid,
			t.session_line_num,
			t.application_name,
			t.user_name,
			t.database_name,
			t.remote_host,
			t.duration,
			s.statement_id,
			t.statement
		FROM tmp_statements t
			INNER JOIN sn_statements s
				ON s.statement_md5 = t.statement_md5
				AND s.statement_norm = t.statement_norm;
	ELSE
		NULL;
	END CASE;
END;
$$;


ALTER FUNCTION template_customer.sn_stat_snapshot_finish(p_snap_id integer) OWNER TO pganalytics;

--
-- Name: sn_sysstat_import(integer, integer); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_sysstat_import(p_customer_id integer, p_server_id integer) RETURNS void
    LANGUAGE plpgsql
    AS $_$
DECLARE
        v_item public.hstore;
        v_snap_tables text[] := array['sn_sysstat_io', 'sn_sysstat_paging', 'sn_sysstat_disks', 'sn_sysstat_hugepages', 'sn_sysstat_network', 'sn_sysstat_cpu', 'sn_sysstat_loadqueue', 'sn_sysstat_memusage', 'sn_sysstat_memstats', 'sn_sysstat_swapusage', 'sn_sysstat_kerneltables', 'sn_sysstat_tasks', 'sn_sysstat_swapstats'];
        v_snap_table text;
        v_table_columns text;
        v_table_columns_types text;
BEGIN
        CREATE TEMP TABLE _tmp_sysstat_snap_cache(
                snap_id integer,
                datetime timestamptz
        ) ON COMMIT DROP;
        FOR v_snap_table IN SELECT unnest(v_snap_tables) LOOP
                -- Check if the temp table exists
                PERFORM 1
                        FROM pg_class
                        WHERE relname = '_tmp_' || v_snap_table
                        AND relnamespace = pg_my_temp_schema();
                IF (NOT FOUND) THEN
                        RAISE WARNING 'Ignoring data for %', v_snap_table;
                        CONTINUE;
                END IF;
                --RAISE NOTICE 'Importing data from %', v_snap_table;
                -- Get column names and type conversion
                EXECUTE
                        $SQL$
                                SELECT
                                string_agg(quote_ident(a.attname), ','),
                                string_agg(
                                        CASE a.atttypid
                                        WHEN 'timestamptz'::regtype THEN
                                                'to_timestamp('||quote_ident(a.attname)||'::double precision)'
                                        WHEN 'double precision'::regtype THEN
                                                'replace('||quote_ident(a.attname)||', '','', ''.'')::double precision'
                                        ELSE
                                                quote_ident(a.attname)||'::'||a.atttypid::regtype::text
                                        END
                                        , ',')
                                FROM pg_attribute a
                                WHERE a.attrelid = $1::regclass
                                AND a.attnum > 0
                                AND NOT a.attisdropped
                                AND (
                                                a.attname = 'snap_id'
                                                OR EXISTS(
                                                                SELECT 1
                                                                FROM pg_attribute atmp
                                                                WHERE atmp.attrelid = $2::regclass
                                                                AND atmp.attname = a.attname
                                                                AND atmp.attnum > 0
                                                                AND NOT atmp.attisdropped)
                                )
                        $SQL$
                        USING v_snap_table, '_tmp_'||v_snap_table
                        INTO v_table_columns, v_table_columns_types;
                EXECUTE
                        format(
                                $SQL$
                                        WITH snap_info AS (
                                                -- Get all the info from this stat_snapshot
                                                SELECT *,
                                                -- Every sysstat item must have been fired at one of
                                                -- (5, 15, ..., 55) minutes. This hack gets the given
                                                -- timestamp and finds its expected source as fired by
                                                -- the crontab script (using minute = 5-55/10)
                                                date_trunc('hour', to_timestamp(t.timestamp::double precision)) + (
                                                        (
                                                                SELECT *
                                                                FROM generate_series(-5, 55, 10) i(mini)
                                                                WHERE i.mini <= EXTRACT(MINUTE FROM to_timestamp(t.timestamp::double precision))
                                                                ORDER BY i.mini DESC
                                                                LIMIT 1
                                                        ) * interval '1minute') AS expected_time
                                                FROM %I AS t
                                        ), new_snaps AS (
                                                -- INSERT INTO sn_stat_snapshot only the snapshots not yet there
                                                -- (ideally it would happen only on the first table imported)
                                                INSERT INTO sn_stat_snapshot(snap_id, snap_type, customer_id, server_id, datetime)
                                                SELECT nextval('sn_stat_snapshot_seq'), 'sysstat', $1, $2, si.expected_time
                                                FROM snap_info si
                                                WHERE NOT EXISTS(
                                                        SELECT 1
                                                        FROM sn_stat_snapshot ss
                                                        WHERE ss.datetime = si.expected_time
                                                                AND ss.customer_id = $1
                                                                AND ss.server_id = $2
                                                )
                                                GROUP BY si.expected_time
                                                RETURNING snap_id, datetime AS expected_time
                                        ), old_snaps AS (
                                                -- Collect snapshots already in sn_stat_snapshot
                                                SELECT ss.snap_id, ss.datetime AS expected_time
                                                FROM sn_stat_snapshot ss
                                                WHERE
                                                        EXISTS(
                                                                SELECT 1
                                                                FROM snap_info si
                                                                WHERE ss.datetime = si.expected_time
                                                        )
                                                        AND ss.customer_id = $1
                                                        AND ss.server_id = $2
                                        )
                                        INSERT INTO %I(%s)
                                        SELECT %s
                                        FROM snap_info si
                                        INNER JOIN (
                                                SELECT * FROM old_snaps
                                                UNION ALL
                                                SELECT * FROM new_snaps
                                        ) ss USING(expected_time)
                                $SQL$,
                                '_tmp_'||v_snap_table,         -- WITH snap_info ...
                                v_snap_table, v_table_columns, -- last INSERT
                                v_table_columns_types          -- last SELECT
                        )
                        USING p_customer_id, p_server_id;
        END LOOP;
        /**
         * XXX: sn_sysstat_import_refresh_mvs is SECURITY INVOKER, so it has
         *      larger permissions, but it also needs SELECT permission on the
         *      temporary tables it collects new data from.
         */
        GRANT SELECT ON _tmp_sn_sysstat_disks TO pganalytics;
        PERFORM sn_sysstat_import_refresh_mvs(p_customer_id, p_server_id);
END;
$_$;


ALTER FUNCTION template_customer.sn_sysstat_import(p_customer_id integer, p_server_id integer) OWNER TO pganalytics;

--
-- Name: sn_sysstat_import(text, text); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_sysstat_import(p_customer_name text, p_server_name text) RETURNS void
    LANGUAGE plpgsql
    AS $$
DECLARE
	v_current_customer_id integer;
	v_server_id integer;
BEGIN
	/* Get and validate the customer_id */
	SELECT c.customer_id INTO v_current_customer_id
	FROM pm_customer_get_current() c
	WHERE c.name_id = p_customer_name;
	IF (v_current_customer_id IS NULL) THEN
		RAISE 'Invalid customer name';
	END IF;
	/* Get and validate the server_id */
	SELECT s.server_id INTO v_server_id
	FROM pm_server s
	WHERE s.name = p_server_name;
	IF (v_server_id IS NULL) THEN
		RAISE 'Invalid server name';
	END IF;
	PERFORM sn_sysstat_import(v_current_customer_id, v_server_id);
END;
$$;


ALTER FUNCTION template_customer.sn_sysstat_import(p_customer_name text, p_server_name text) OWNER TO pganalytics;

--
-- Name: sn_sysstat_import_refresh_mvs(integer, integer); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) RETURNS void
    LANGUAGE plpgsql SECURITY DEFINER
    AS $$
BEGIN
	-- Update materialized view mv_sn_sysstat_disks_devs (we need to LOCK it for concurrent write, as it is a poor-MERGE)
	LOCK TABLE mv_sn_sysstat_disks_devs IN EXCLUSIVE MODE;
	INSERT INTO mv_sn_sysstat_disks_devs
	SELECT DISTINCT p_server_id, d.dev
	FROM _tmp_sn_sysstat_disks d
	WHERE NOT EXISTS(SELECT 1 FROM mv_sn_sysstat_disks_devs m WHERE m.server_id = p_server_id AND m.dev = d.dev);
END;
$$;


ALTER FUNCTION template_customer.sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) OWNER TO pganalytics;

--
-- Name: sn_tablespace_update_fsdevice(name, text); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) RETURNS void
    LANGUAGE sql SECURITY DEFINER
    AS $$
	UPDATE sn_tablespace
	SET fsdevice = p_fsdevice
	WHERE snap_id = currval('sn_stat_snapshot_seq')
		AND spcname = p_spcname;
$$;


ALTER FUNCTION template_customer.sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) OWNER TO pganalytics;

--
-- Name: tg_pm_pglog_ignore(); Type: FUNCTION; Schema: template_customer; Owner: pganalytics
--

CREATE FUNCTION tg_pm_pglog_ignore() RETURNS trigger
    LANGUAGE plpgsql
    AS $$
BEGIN
	IF (EXISTS(SELECT 1
			FROM pm_pglog_ignore i
			WHERE
				(i.normalized_regexp IS NULL OR NEW.normalized ~ i.normalized_regexp)
				AND (i.message_regexp IS NULL OR NEW.message ~ i.message_regexp)
				AND (i.error_severity_regexp IS NULL OR NEW.error_severity ~ i.error_severity_regexp)
				AND (i.user_name_regexp IS NULL OR NEW.user_name ~ i.user_name_regexp)
				AND (i.application_name_regexp IS NULL OR NEW.application_name ~ i.application_name_regexp)
				AND (i.database_name_regexp IS NULL OR NEW.database_name ~ i.database_name_regexp)
				AND (i.remote_host_regexp IS NULL OR NEW.remote_host ~ i.remote_host_regexp)
	)) THEN
		RETURN NULL;
	END IF;
	RETURN NEW;
END;
$$;


ALTER FUNCTION template_customer.tg_pm_pglog_ignore() OWNER TO pganalytics;

--
-- Name: alert; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW alert AS
 SELECT alert.alert_id,
    alert.alert_item,
    alert.alert_value,
    alert.alert_time,
    alert.customer_id,
    alert.server_id,
    alert.instance_id,
    alert.database_id,
    alert.alert_sent_time,
    alert.job_id,
    alert.alert_severity,
    alert.jobdata_id,
    alert.alert_hit,
    alert.alert_url,
    alert.customer_datetime
   FROM pganalytics.alert
  WHERE (alert.customer_id = ( SELECT (pm_customer_get_current()).customer_id AS customer_id));


ALTER TABLE alert OWNER TO pganalytics;

SET default_tablespace = '';

SET default_with_oids = false;

--
-- Name: mv_sn_sysstat_disks_devs; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE mv_sn_sysstat_disks_devs (
    server_id integer NOT NULL,
    dev text NOT NULL
);


ALTER TABLE mv_sn_sysstat_disks_devs OWNER TO pganalytics;

--
-- Name: sn_stat_snapshot; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_snapshot (
    snap_id integer NOT NULL,
    customer_id integer NOT NULL,
    server_id integer NOT NULL,
    instance_id integer,
    datetime timestamp with time zone,
    snap_type snap_type_enum,
    datname text,
    obs text,
    database_id integer,
    snap_type_id integer,
    snap_hash text,
    real_datetime timestamp with time zone,
    CONSTRAINT chk_verify_refs CHECK (((database_id IS NULL) OR (instance_id IS NOT NULL)))
);


ALTER TABLE sn_stat_snapshot OWNER TO pganalytics;

--
-- Name: sn_stat_snapshot_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE sn_stat_snapshot_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE sn_stat_snapshot_seq OWNER TO pganalytics;

--
-- Name: sn_stat_snapshot_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE sn_stat_snapshot_seq OWNED BY sn_stat_snapshot.snap_id;


--
-- Name: sn_pglog; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_pglog (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    application_name text,
    user_name text,
    database_name text,
    remote_host_port text,
    remote_host text,
    pid integer,
    log_time timestamp with time zone,
    log_time_ms timestamp with time zone,
    command_tag text,
    sql_state_code text,
    session_id text,
    session_line_num bigint,
    session_start_time timestamp with time zone,
    virtual_transaction_id text,
    transaction_id bigint,
    error_severity text,
    message text,
    normalized text
);


ALTER TABLE sn_pglog OWNER TO pganalytics;

--
-- Name: vw_sn_pglog_autovacuum; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_pglog_autovacuum AS
SELECT c.server_id,
    c.instance_id,
    c.log_time,
    c.pid,
    c.session_line_num,
    c.matches[1] AS tablename,
    (c.matches[2])::integer AS idx_scans,
    (c.matches[3])::integer AS pages_removed,
    (c.matches[4])::integer AS pages_remain,
    (c.matches[5])::integer AS tuples_removed,
    (c.matches[6])::integer AS tuples_remain,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[8])::integer
            ELSE (c.matches[7])::integer
        END AS buffer_hit,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[9])::integer
            ELSE (c.matches[8])::integer
        END AS buffer_miss,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[10])::integer
            ELSE (c.matches[9])::integer
        END AS buffer_dirtied,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[11])::numeric
            ELSE (c.matches[10])::numeric
        END AS avg_read_rate_mbs,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[12])::numeric
            ELSE (c.matches[11])::numeric
        END AS avg_write_rate_mbs,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[13])::numeric
            ELSE (c.matches[12])::numeric
        END AS cpu_sys_sec,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[14])::numeric
            ELSE (c.matches[13])::numeric
        END AS cpu_user_sec,
        CASE
            WHEN (c.is_newer_log = 'true'::text) THEN (c.matches[15])::numeric
            ELSE (c.matches[14])::numeric
        END AS duration_sec
   FROM ( SELECT sss.server_id,
            sss.instance_id,
            l.log_time,
            l.pid,
            l.session_line_num,
                CASE
                    WHEN (l.message ~~ '%removable%'::text) THEN 'true'::text
                    ELSE 'false'::text
                END AS is_newer_log,
                CASE
                    WHEN (l.message ~~ '%removable%'::text) THEN regexp_matches(replace(l.message, '
'::text, ';'::text), ((((((('automatic vacuum of table "(.*)": index scans: ([0-9]+);'::text || 'pages: ([0-9]+) removed, ([0-9]+) remain;'::text) || 'tuples: ([0-9]+) removed, ([0-9]+) remain, ([0-9]+) are dead but not yet removable;'::text) || '(?:'::text) || 'buffer usage: ([0-9]+) hits, ([0-9]+) misses, ([0-9]+) dirtied;'::text) || 'avg read rate: ([0-9]+\.[0-9]+) Mi?B\/s, avg write rate: ([0-9]+\.[0-9]+) Mi?B\/s;'::text) || ')?'::text) || 'system usage: CPU ([0-9]+\.[0-9]+)s/([0-9]+\.[0-9]+)u sec elapsed ([0-9]+\.[0-9]+) sec'::text))
                    ELSE regexp_matches(replace(l.message, '
'::text, ';'::text), ((((((('automatic vacuum of table "(.*)": index scans: ([0-9]+);'::text || 'pages: ([0-9]+) removed, ([0-9]+) remain;'::text) || 'tuples: ([0-9]+) removed, ([0-9]+) remain;'::text) || '(?:'::text) || 'buffer usage: ([0-9]+) hits, ([0-9]+) misses, ([0-9]+) dirtied;'::text) || 'avg read rate: ([0-9]+\.[0-9]+) Mi?B\/s, avg write rate: ([0-9]+\.[0-9]+) Mi?B\/s;'::text) || ')?'::text) || 'system usage: CPU ([0-9]+\.[0-9]+)s/([0-9]+\.[0-9]+)u sec elapsed ([0-9]+\.[0-9]+) sec'::text))
                END AS matches
           FROM (template_customer.sn_pglog l
             JOIN template_customer.sn_stat_snapshot sss ON ((sss.snap_id = l.snap_id)))
          WHERE ((l.normalized ~~ 'automatic vacuum of table %'::text) AND (l.error_severity = 'LOG'::text))) c;

ALTER TABLE vw_sn_pglog_autovacuum OWNER TO pganalytics;

--
-- Name: mvw_pglog_autovacuum; Type: MATERIALIZED VIEW; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE MATERIALIZED VIEW mvw_pglog_autovacuum AS
 SELECT row_number() OVER (ORDER BY vw_sn_pglog_autovacuum.log_time) AS row_number,
    vw_sn_pglog_autovacuum.server_id,
    vw_sn_pglog_autovacuum.instance_id,
    vw_sn_pglog_autovacuum.log_time,
    split_part(vw_sn_pglog_autovacuum.tablename, '.'::text, 1) AS datname,
    ((split_part(vw_sn_pglog_autovacuum.tablename, '.'::text, 2) || '.'::text) || split_part(vw_sn_pglog_autovacuum.tablename, '.'::text, 3)) AS tablename,
    vw_sn_pglog_autovacuum.pages_removed,
    vw_sn_pglog_autovacuum.pages_remain,
    vw_sn_pglog_autovacuum.tuples_removed,
    vw_sn_pglog_autovacuum.tuples_remain,
    vw_sn_pglog_autovacuum.buffer_hit,
    vw_sn_pglog_autovacuum.buffer_miss,
    vw_sn_pglog_autovacuum.buffer_dirtied,
    vw_sn_pglog_autovacuum.avg_read_rate_mbs,
    vw_sn_pglog_autovacuum.avg_write_rate_mbs,
    vw_sn_pglog_autovacuum.cpu_sys_sec,
    vw_sn_pglog_autovacuum.cpu_user_sec,
    vw_sn_pglog_autovacuum.duration_sec
   FROM vw_sn_pglog_autovacuum
  WITH NO DATA;


ALTER TABLE mvw_pglog_autovacuum OWNER TO pganalytics;

--
-- Name: vw_sn_pglog_checkpoints; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_pglog_checkpoints AS
 SELECT c.server_id,
    c.instance_id,
    c.log_time,
    c.pid,
    c.session_line_num,
    (NULLIF(c.matches[1], ''::text))::integer AS num_buffers,
    (NULLIF(c.matches[2], ''::text))::numeric AS perc_buffers,
    (c.matches[3])::integer AS xlog_added,
    (c.matches[4])::integer AS xlog_removed,
    (c.matches[5])::integer AS xlog_recycled,
    (NULLIF(c.matches[6], ''::text))::numeric AS write_time,
    (NULLIF(c.matches[7], ''::text))::numeric AS sync_time,
    (NULLIF(c.matches[8], ''::text))::numeric AS total_time,
    (NULLIF(c.matches[9], ''::text))::integer AS sync_files,
    (NULLIF(c.matches[10], ''::text))::numeric AS longest_sync_file_time,
    (NULLIF(c.matches[11], ''::text))::numeric AS avg_sync_file_time
   FROM ( SELECT sss.server_id,
            sss.instance_id,
            l.log_time,
            l.pid,
            l.session_line_num,
            regexp_matches(l.message, (((('^checkpoint complete[;:]'::text || '(?: wrote ([0-9]+) buffers \(([0-9]+\.[0-9]+)%\);)?'::text) || ' ([0-9]+) transaction log file\(s\) added, ([0-9]+) removed, ([0-9]+) recycled'::text) || '(?:; write=([0-9]+\.[0-9]+) s, sync=([0-9]+\.[0-9]+) s, total=([0-9]+\.[0-9]+) s)?'::text) || '(?:; sync files=([0-9]+), longest=([0-9]+\.[0-9]+) s, average=([0-9]+\.[0-9]+) s)?'::text)) AS matches
           FROM (sn_pglog l
             JOIN sn_stat_snapshot sss ON ((sss.snap_id = l.snap_id)))
          WHERE ((l.normalized ~~ 'checkpoint complete:%'::text) AND (l.error_severity = 'LOG'::text))) c;


ALTER TABLE vw_sn_pglog_checkpoints OWNER TO pganalytics;

--
-- Name: mvw_pglog_checkpoint; Type: MATERIALIZED VIEW; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE MATERIALIZED VIEW mvw_pglog_checkpoint AS
 SELECT row_number() OVER (ORDER BY vw_sn_pglog_checkpoints.log_time) AS row_number,
    vw_sn_pglog_checkpoints.server_id,
    vw_sn_pglog_checkpoints.instance_id,
    vw_sn_pglog_checkpoints.log_time,
    vw_sn_pglog_checkpoints.pid,
    vw_sn_pglog_checkpoints.session_line_num,
    vw_sn_pglog_checkpoints.num_buffers,
    vw_sn_pglog_checkpoints.perc_buffers,
    vw_sn_pglog_checkpoints.xlog_added,
    vw_sn_pglog_checkpoints.xlog_removed,
    vw_sn_pglog_checkpoints.xlog_recycled,
    vw_sn_pglog_checkpoints.write_time,
    vw_sn_pglog_checkpoints.sync_time,
    vw_sn_pglog_checkpoints.total_time,
    vw_sn_pglog_checkpoints.sync_files,
    vw_sn_pglog_checkpoints.longest_sync_file_time,
    vw_sn_pglog_checkpoints.avg_sync_file_time
   FROM vw_sn_pglog_checkpoints
  WITH NO DATA;


ALTER TABLE mvw_pglog_checkpoint OWNER TO pganalytics;

--
-- Name: pm_database; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE pm_database (
    database_id integer NOT NULL,
    instance_id integer NOT NULL,
    name text
);


ALTER TABLE pm_database OWNER TO pganalytics;

--
-- Name: pm_database_database_id_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE pm_database_database_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE pm_database_database_id_seq OWNER TO pganalytics;

--
-- Name: pm_database_database_id_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE pm_database_database_id_seq OWNED BY pm_database.database_id;


--
-- Name: pm_instance; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE pm_instance (
    instance_id integer NOT NULL,
    server_id integer NOT NULL,
    port integer,
    name text,
    master_id integer
);


ALTER TABLE pm_instance OWNER TO pganalytics;

--
-- Name: pm_instance_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE pm_instance_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE pm_instance_seq OWNER TO pganalytics;

--
-- Name: pm_instance_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE pm_instance_seq OWNED BY pm_instance.instance_id;


--
-- Name: pm_pglog_ignore; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE pm_pglog_ignore (
    normalized_regexp text,
    message_regexp text,
    error_severity_regexp text,
    user_name_regexp text,
    application_name_regexp text,
    database_name_regexp text,
    remote_host_regexp text
);


ALTER TABLE pm_pglog_ignore OWNER TO pganalytics;

--
-- Name: pm_server; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE pm_server (
    server_id integer NOT NULL,
    customer_id integer NOT NULL,
    name text,
    description text
);


ALTER TABLE pm_server OWNER TO pganalytics;

--
-- Name: pm_server_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE pm_server_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE pm_server_seq OWNER TO pganalytics;

--
-- Name: pm_server_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE pm_server_seq OWNED BY pm_server.server_id;


--
-- Name: pm_snap_type; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE pm_snap_type (
    snap_type_id integer NOT NULL,
    snap_type_desc text
);


ALTER TABLE pm_snap_type OWNER TO pganalytics;

--
-- Name: pm_snap_type_snap_type_id_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE pm_snap_type_snap_type_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE pm_snap_type_snap_type_id_seq OWNER TO pganalytics;

--
-- Name: pm_snap_type_snap_type_id_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE pm_snap_type_snap_type_id_seq OWNED BY pm_snap_type.snap_type_id;


--
-- Name: pm_view; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW pm_view AS
 SELECT c.customer_id,
    c.name AS customer_name,
    s.server_id,
    s.name AS server_name,
    i.instance_id,
    i.name AS instance_name,
    i.port AS instance_port,
    d.database_id,
    d.name AS database_name
   FROM (((pm_database d
     JOIN pm_instance i USING (instance_id))
     JOIN pm_server s USING (server_id))
     JOIN pganalytics.pm_customer c USING (customer_id));


ALTER TABLE pm_view OWNER TO pganalytics;

--
-- Name: sn_data_info; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_data_info (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    data_key text NOT NULL,
    data_value text
);


ALTER TABLE sn_data_info OWNER TO pganalytics;

--
-- Name: sn_database; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_database (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    datname text,
    datdba oid,
    encoding integer,
    datcollate text,
    datctype text,
    datistemplate boolean,
    datallowconn boolean,
    datconnlimit integer,
    datlastsysoid oid,
    datfrozenxid xid,
    datminmxid xid,
    dattablespace oid,
    dbsize bigint,
    age_datfrozenxid integer,
    datacl text,
    datconfig text[]
);


ALTER TABLE sn_database OWNER TO pganalytics;

--
-- Name: sn_diagnostic; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_diagnostic (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    diagnostic text,
    recomendation text,
    priority text,
    is_automatic boolean,
    autor text,
    expire tstzrange
);


ALTER TABLE sn_diagnostic OWNER TO pganalytics;

--
-- Name: sn_disk_usage; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_disk_usage (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    fsdevice text,
    fstype text,
    size bigint,
    used bigint,
    available bigint,
    usage text,
    mountpoint text
);


ALTER TABLE sn_disk_usage OWNER TO pganalytics;

--
-- Name: sn_instance; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_instance (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    pg_postmaster_start_time timestamp with time zone,
    pg_current_xlog_insert_location text
);


ALTER TABLE sn_instance OWNER TO pganalytics;

--
-- Name: sn_namespace; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_namespace (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    nspname text,
    nspowner oid,
    nspacl text
);


ALTER TABLE sn_namespace OWNER TO pganalytics;

--
-- Name: sn_relations; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_relations (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    relname text,
    nspname text,
    relfilenode oid,
    spcname text,
    relpages oid,
    reltuples real,
    relkind character(1),
    relfrozenxid xid,
    relacl text,
    reloptions text[],
    relsize bigint,
    age_relfrozenxid integer
);


ALTER TABLE sn_relations OWNER TO pganalytics;

--
-- Name: sn_settings; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_settings (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    name text,
    setting text
);


ALTER TABLE sn_settings OWNER TO pganalytics;

--
-- Name: sn_stat_archiver; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_archiver (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    archived_count bigint,
    last_archived_wal text,
    last_archived_time timestamp with time zone,
    failed_count bigint,
    last_failed_wal text,
    last_failed_time timestamp with time zone,
    stats_reset timestamp with time zone
);


ALTER TABLE sn_stat_archiver OWNER TO pganalytics;

--
-- Name: sn_stat_bgwriter; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_bgwriter (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    checkpoints_timed bigint,
    checkpoints_req bigint,
    checkpoint_write_time double precision,
    checkpoint_sync_time double precision,
    buffers_checkpoint bigint,
    buffers_clean bigint,
    maxwritten_clean bigint,
    buffers_backend bigint,
    buffers_backend_fsync bigint,
    buffers_alloc bigint,
    stats_reset timestamp with time zone
);


ALTER TABLE sn_stat_bgwriter OWNER TO pganalytics;

--
-- Name: sn_stat_database; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_database (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    datid oid,
    datname text,
    numbackends integer,
    xact_commit bigint,
    xact_rollback bigint,
    blks_read bigint,
    blks_hit bigint,
    tup_returned bigint,
    tup_fetched bigint,
    tup_inserted bigint,
    tup_updated bigint,
    tup_deleted bigint,
    conflicts bigint,
    temp_files bigint,
    temp_bytes bigint,
    deadlocks bigint,
    blk_read_time double precision,
    blk_write_time double precision,
    stats_reset timestamp with time zone
);


ALTER TABLE sn_stat_database OWNER TO pganalytics;

--
-- Name: sn_stat_database_conflicts; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_database_conflicts (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    datid bigint,
    datname text,
    confl_tablespace bigint,
    confl_lock bigint,
    confl_snapshot bigint,
    confl_bufferpin bigint,
    confl_deadlock bigint
);


ALTER TABLE sn_stat_database_conflicts OWNER TO pganalytics;

--
-- Name: sn_stat_replication; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_replication (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    sender_location text,
    pid integer,
    usesysid bigint,
    usename text,
    application_name text,
    client_addr inet,
    client_hostname text,
    client_port integer,
    backend_start timestamp with time zone,
    backend_xmin xid,
    state text,
    sent_location text,
    write_location text,
    flush_location text,
    replay_location text,
    sync_priority integer,
    sync_state text,
    sent_location_diff bigint,
    write_location_diff bigint,
    flush_location_diff bigint,
    replay_location_diff bigint
);


ALTER TABLE sn_stat_replication OWNER TO pganalytics;

--
-- Name: sn_stat_user_functions; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_user_functions (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    funcid oid,
    schemaname text,
    funcname text,
    calls bigint,
    total_time double precision,
    self_time double precision
);


ALTER TABLE sn_stat_user_functions OWNER TO pganalytics;

--
-- Name: sn_stat_user_indexes; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_user_indexes (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    indexrelid oid,
    schemaname text,
    relname text,
    indexrelname text,
    idx_scan bigint,
    idx_tup_read bigint,
    idx_tup_fetch bigint
);


ALTER TABLE sn_stat_user_indexes OWNER TO pganalytics;

--
-- Name: sn_stat_user_tables; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stat_user_tables (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    schemaname text,
    relname text,
    seq_scan bigint,
    seq_tup_read bigint,
    idx_scan bigint,
    idx_tup_fetch bigint,
    n_tup_ins bigint,
    n_tup_upd bigint,
    n_tup_del bigint,
    n_tup_hot_upd bigint,
    n_live_tup bigint,
    n_dead_tup bigint,
    last_vacuum timestamp with time zone,
    last_autovacuum timestamp with time zone,
    last_analyze timestamp with time zone,
    last_autoanalyze timestamp with time zone,
    vacuum_count bigint,
    autovacuum_count bigint,
    analyze_count bigint,
    autoanalyze_count bigint,
    n_mod_since_analyze bigint
);


ALTER TABLE sn_stat_user_tables OWNER TO pganalytics;

--
-- Name: sn_statements; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_statements (
    statement_id bigint NOT NULL,
    statement_md5 bytea,
    statement_norm text
);


ALTER TABLE sn_statements OWNER TO pganalytics;

--
-- Name: sn_statements_executed; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_statements_executed (
    snap_id integer,
    server_id integer,
    instance_id integer,
    database_id integer,
    log_time timestamp with time zone,
    pid integer,
    session_line_num bigint,
    application_name text,
    user_name text,
    database_name text,
    remote_host text,
    duration double precision,
    statement_id integer,
    statement text
);


ALTER TABLE sn_statements_executed OWNER TO pganalytics;

--
-- Name: sn_statements_statement_id_seq; Type: SEQUENCE; Schema: template_customer; Owner: pganalytics
--

CREATE SEQUENCE sn_statements_statement_id_seq
    START WITH 1
    INCREMENT BY 1
    NO MINVALUE
    NO MAXVALUE
    CACHE 1;


ALTER TABLE sn_statements_statement_id_seq OWNER TO pganalytics;

--
-- Name: sn_statements_statement_id_seq; Type: SEQUENCE OWNED BY; Schema: template_customer; Owner: pganalytics
--

ALTER SEQUENCE sn_statements_statement_id_seq OWNED BY sn_statements.statement_id;


--
-- Name: sn_statio_user_indexes; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_statio_user_indexes (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    indexrelid oid,
    schemaname text,
    relname text,
    indexrelname text,
    idx_blks_read bigint,
    idx_blks_hit bigint
);


ALTER TABLE sn_statio_user_indexes OWNER TO pganalytics;

--
-- Name: sn_statio_user_sequences; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_statio_user_sequences (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    schemaname text,
    relname text,
    blks_read bigint,
    blks_hit bigint
);


ALTER TABLE sn_statio_user_sequences OWNER TO pganalytics;

--
-- Name: sn_statio_user_tables; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_statio_user_tables (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    relid oid,
    schemaname text,
    relname text,
    heap_blks_read bigint,
    heap_blks_hit bigint,
    idx_blks_read bigint,
    idx_blks_hit bigint,
    toast_blks_read bigint,
    toast_blks_hit bigint,
    tidx_blks_read bigint,
    tidx_blks_hit bigint
);


ALTER TABLE sn_statio_user_tables OWNER TO pganalytics;

--
-- Name: sn_stats; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_stats (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    schemaname text,
    tablename text,
    attname text,
    inherited boolean,
    null_frac real,
    avg_width integer,
    n_distinct real,
    most_common_vals text[],
    most_common_freqs real[],
    histogram_bounds text[],
    correlation real,
    most_common_elems text[],
    most_common_elem_freqs real[],
    elem_count_histogram real[]
);


ALTER TABLE sn_stats OWNER TO pganalytics;

--
-- Name: sn_sysstat_cpu; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_cpu (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    cpu integer NOT NULL,
    _user double precision NOT NULL,
    _nice double precision,
    _system double precision NOT NULL,
    _iowait double precision,
    _steal double precision,
    _idle double precision NOT NULL
);


ALTER TABLE sn_sysstat_cpu OWNER TO pganalytics;

--
-- Name: sn_sysstat_disks; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_disks (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    dev text NOT NULL,
    tps double precision NOT NULL,
    rd_sec_s double precision NOT NULL,
    wr_sec_s double precision NOT NULL,
    avgrq_sz double precision NOT NULL,
    avgqu_sz double precision,
    await double precision,
    svctm double precision,
    _util double precision NOT NULL
);


ALTER TABLE sn_sysstat_disks OWNER TO pganalytics;

--
-- Name: sn_sysstat_hugepages; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_hugepages (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" text NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    kbhugfree double precision NOT NULL,
    kbhugused double precision NOT NULL,
    _hugused double precision NOT NULL
);


ALTER TABLE sn_sysstat_hugepages OWNER TO pganalytics;

--
-- Name: sn_sysstat_io; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_io (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    tps double precision NOT NULL,
    rtps double precision NOT NULL,
    wtps double precision NOT NULL,
    bread_s double precision NOT NULL,
    bwrtn_s double precision NOT NULL
);


ALTER TABLE sn_sysstat_io OWNER TO pganalytics;

--
-- Name: sn_sysstat_kerneltables; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_kerneltables (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    dentunusd integer NOT NULL,
    file_nr integer NOT NULL,
    inode_nr integer NOT NULL,
    pty_nr integer NOT NULL
);


ALTER TABLE sn_sysstat_kerneltables OWNER TO pganalytics;

--
-- Name: sn_sysstat_loadqueue; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_loadqueue (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    runq_sz integer NOT NULL,
    plist_sz integer NOT NULL,
    ldavg_1 double precision NOT NULL,
    ldavg_5 double precision NOT NULL,
    ldavg_15 double precision NOT NULL,
    blocked integer
);


ALTER TABLE sn_sysstat_loadqueue OWNER TO pganalytics;

--
-- Name: sn_sysstat_memstats; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_memstats (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    frmpg_s double precision NOT NULL,
    bufpg_s double precision NOT NULL,
    campg_s double precision NOT NULL
);


ALTER TABLE sn_sysstat_memstats OWNER TO pganalytics;

--
-- Name: sn_sysstat_memusage; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_memusage (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    kbmemfree double precision NOT NULL,
    kbmemused double precision NOT NULL,
    _memused double precision NOT NULL,
    kbbuffers double precision,
    kbcached double precision NOT NULL,
    kbcommit double precision NOT NULL,
    _commit double precision NOT NULL,
    kbactive double precision,
    kbinact double precision
);


ALTER TABLE sn_sysstat_memusage OWNER TO pganalytics;

--
-- Name: sn_sysstat_network; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_network (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    iface text NOT NULL,
    rxpck_s double precision NOT NULL,
    txpck_s double precision NOT NULL,
    rxkb_s double precision NOT NULL,
    txkb_s double precision NOT NULL,
    rxcmp_s double precision NOT NULL,
    txcmp_s double precision NOT NULL,
    rxmcst_s double precision NOT NULL
);


ALTER TABLE sn_sysstat_network OWNER TO pganalytics;

--
-- Name: sn_sysstat_paging; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_paging (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    pgpgin_s double precision NOT NULL,
    pgpgout_s double precision NOT NULL,
    fault_s double precision NOT NULL,
    majflt_s double precision NOT NULL,
    pgfree_s double precision NOT NULL,
    pgscank_s double precision NOT NULL,
    pgscand_s double precision NOT NULL,
    pgsteal_s double precision NOT NULL,
    _vmeff double precision NOT NULL
);


ALTER TABLE sn_sysstat_paging OWNER TO pganalytics;

--
-- Name: sn_sysstat_swapstats; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_swapstats (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    pswpin_s double precision NOT NULL,
    pswpout_s double precision NOT NULL
);


ALTER TABLE sn_sysstat_swapstats OWNER TO pganalytics;

--
-- Name: sn_sysstat_swapusage; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_swapusage (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    kbswpfree double precision NOT NULL,
    kbswpused double precision NOT NULL,
    _swpused double precision NOT NULL,
    kbswpcad double precision NOT NULL,
    _swpcad double precision NOT NULL
);


ALTER TABLE sn_sysstat_swapusage OWNER TO pganalytics;

--
-- Name: sn_sysstat_tasks; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_sysstat_tasks (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass) NOT NULL,
    hostname text NOT NULL,
    "interval" integer NOT NULL,
    "timestamp" timestamp with time zone NOT NULL,
    proc_s double precision NOT NULL,
    cswch_s double precision NOT NULL
);


ALTER TABLE sn_sysstat_tasks OWNER TO pganalytics;

--
-- Name: sn_tablespace; Type: TABLE; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE TABLE sn_tablespace (
    snap_id integer DEFAULT currval('sn_stat_snapshot_seq'::regclass),
    spcname text,
    spcowner oid,
    spcacl text,
    spcoptions text[],
    spcsize bigint,
    spcid oid,
    spclocation text,
    fsdevice text
);


ALTER TABLE sn_tablespace OWNER TO pganalytics;

--
-- Name: vw_backup_list; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_backup_list WITH (security_barrier=true) AS
 SELECT max(calc.backup_begin) OVER y AS max,
    calc.datname,
    calc.backup_begin,
    calc.duration,
    calc.duration_diff,
    calc.size,
    calc.size_diff,
    calc.parts,
    calc.parts_diff,
    calc.customer_id,
    calc.server_id,
    calc.instance_id,
    calc.database_id
   FROM ( SELECT sn_last_backup.datname,
            (sn_last_backup.backup_begin)::timestamp without time zone AS backup_begin,
            sn_last_backup.duration,
            (sn_last_backup.duration - lag(sn_last_backup.duration) OVER w) AS duration_diff,
            sn_last_backup.backup_size AS size,
            (sn_last_backup.backup_size - lag(sn_last_backup.backup_size) OVER w) AS size_diff,
            sn_last_backup.parts,
            (sn_last_backup.parts - lag(sn_last_backup.parts) OVER w) AS parts_diff,
            sn_last_backup.customer_id,
            sn_last_backup.server_id,
            sn_last_backup.instance_id,
            sn_last_backup.database_id
           FROM ( SELECT DISTINCT backup_info.datname,
                    replace(backup_info.backup_file, backup_info.backup_part, '*'::text) AS backup_file,
                    min(backup_info.backup_begin) OVER k AS backup_begin,
                    max(backup_info.backup_end) OVER k AS backup_end,
                    sum((backup_info.duration)::interval) OVER k AS duration,
                    sum(backup_info.backup_size) OVER k AS backup_size,
                    count(*) OVER k AS parts,
                    backup_info.customer_id,
                    backup_info.server_id,
                    backup_info.instance_id,
                    backup_info.database_id
                   FROM ( SELECT ss1.datname,
                            to_char((snd.backup_begin)::timestamp without time zone, 'DD/MM/YYYY HH24:MI:SS'::text) AS backup_begin,
                            to_char((snd.backup_end)::timestamp without time zone, 'DD/MM/YYYY HH24:MI:SS'::text) AS backup_end,
                            to_char(date_trunc('second'::text, age((snd.backup_end)::timestamp without time zone, (snd.backup_begin)::timestamp without time zone)), 'HH24:MI:SS'::text) AS duration,
                            snd.backup_file,
                            snd.backup_part,
                            (snd.backup_size)::bigint AS backup_size,
                            ss1.customer_id,
                            ss1.server_id,
                            ss1.instance_id,
                            ss1.database_id
                           FROM (public.crosstab('SELECT snap_id,data_key,data_value FROM sn_data_info ORDER BY snap_id'::text, 'SELECT DISTINCT data_key FROM sn_data_info ORDER BY data_key'::text) snd(snap_id bigint, backup_begin text, backup_end text, backup_file text, backup_part text, backup_size text)
                             JOIN sn_stat_snapshot ss1 USING (snap_id))
                          WHERE (ss1.snap_type = 'pg_dump'::snap_type_enum)
                          ORDER BY (snd.backup_begin)::timestamp without time zone DESC) backup_info
                  WINDOW k AS (PARTITION BY replace(backup_info.backup_file, backup_info.backup_part, '*'::text))
                  ORDER BY max(backup_info.backup_end) OVER k DESC) sn_last_backup
          WINDOW w AS (PARTITION BY sn_last_backup.datname ORDER BY date_trunc('day'::text, (sn_last_backup.backup_begin)::timestamp without time zone))) calc
  WHERE (calc.parts IS NOT NULL)
  WINDOW y AS (PARTITION BY calc.datname);


ALTER TABLE vw_backup_list OWNER TO pganalytics;

--
-- Name: vw_sn_last_snapshots; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_last_snapshots AS
 WITH last AS (
         SELECT DISTINCT ON (sn_stat_snapshot.customer_id, sn_stat_snapshot.server_id, COALESCE(sn_stat_snapshot.instance_id, 0), COALESCE(sn_stat_snapshot.database_id, 0), sn_stat_snapshot.snap_type) sn_stat_snapshot.customer_id,
            sn_stat_snapshot.server_id,
            COALESCE(sn_stat_snapshot.instance_id, 0) AS instance_id,
            COALESCE(sn_stat_snapshot.database_id, 0) AS database_id,
            sn_stat_snapshot.snap_type,
            sn_stat_snapshot.snap_id AS last_snap_id,
            sn_stat_snapshot.datetime AS last_datetime
           FROM sn_stat_snapshot
          ORDER BY sn_stat_snapshot.customer_id, sn_stat_snapshot.server_id, COALESCE(sn_stat_snapshot.instance_id, 0), COALESCE(sn_stat_snapshot.database_id, 0), sn_stat_snapshot.snap_type, sn_stat_snapshot.datetime DESC
        ), last_but_one AS (
         SELECT DISTINCT ON (sn_stat_snapshot.customer_id, sn_stat_snapshot.server_id, COALESCE(sn_stat_snapshot.instance_id, 0), COALESCE(sn_stat_snapshot.database_id, 0), sn_stat_snapshot.snap_type) sn_stat_snapshot.customer_id,
            sn_stat_snapshot.server_id,
            COALESCE(sn_stat_snapshot.instance_id, 0) AS instance_id,
            COALESCE(sn_stat_snapshot.database_id, 0) AS database_id,
            sn_stat_snapshot.snap_type,
            sn_stat_snapshot.snap_id AS last_snap_id,
            sn_stat_snapshot.datetime AS last_datetime
           FROM sn_stat_snapshot
          WHERE (NOT (EXISTS ( SELECT 1
                   FROM last last_1
                  WHERE (last_1.last_snap_id = sn_stat_snapshot.snap_id))))
          ORDER BY sn_stat_snapshot.customer_id, sn_stat_snapshot.server_id, COALESCE(sn_stat_snapshot.instance_id, 0), COALESCE(sn_stat_snapshot.database_id, 0), sn_stat_snapshot.snap_type, sn_stat_snapshot.datetime DESC
        )
 SELECT last.customer_id,
    last.server_id,
    last.instance_id,
    last.database_id,
    last.snap_type,
    last.last_snap_id,
    last.last_datetime,
    last_but_one.last_snap_id AS lbo_snap_id,
    last_but_one.last_datetime AS lbo_datetime
   FROM (last
     JOIN last_but_one USING (customer_id, server_id, instance_id, database_id, snap_type));


ALTER TABLE vw_sn_last_snapshots OWNER TO pganalytics;

--
-- Name: vw_sn_last_stat_snapshot; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_last_stat_snapshot AS
 SELECT n.snap_id
   FROM ( SELECT sn_stat_snapshot.snap_id,
            row_number() OVER (PARTITION BY sn_stat_snapshot.database_id ORDER BY sn_stat_snapshot.datetime DESC) AS rn
           FROM sn_stat_snapshot) n
  WHERE (n.rn <= 5);


ALTER TABLE vw_sn_last_stat_snapshot OWNER TO pganalytics;

--
-- Name: vw_sn_pglog_locks; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_pglog_locks AS
 SELECT c.server_id,
    c.instance_id,
    c.log_time,
    c.pid,
    c.session_line_num,
    c.matches[2] AS mode,
    c.matches[3] AS object,
    (c.matches[4])::numeric AS duration
   FROM ( SELECT sss.server_id,
            sss.instance_id,
            l.log_time,
            l.pid,
            l.session_line_num,
            regexp_matches(l.message, '^process ([\d]+) acquired ([^ ]+) on (.*) after (\d+\.\d+) ms$'::text) AS matches
           FROM (sn_pglog l
             JOIN sn_stat_snapshot sss ON ((sss.snap_id = l.snap_id)))
          WHERE ((l.normalized ~~ 'process ? acquired %Lock on % after ? ms'::text) AND (l.error_severity = 'LOG'::text))) c;


ALTER TABLE vw_sn_pglog_locks OWNER TO pganalytics;

--
-- Name: vw_sn_pglog_statements; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_pglog_statements AS
 SELECT sss.server_id,
    sss.instance_id,
    d.database_id,
    l.log_time,
    l.pid,
    l.session_line_num,
    l.application_name,
    l.user_name,
    l.database_name,
    l.remote_host,
    ("substring"(l.message, '^duration: (\d+\.\d+) ms'::text))::double precision AS duration,
    "substring"(l.message, '^duration: \d+\.\d+ ms  (?:statement|execute <unnamed>): (.*)'::text) AS statement,
    "substring"(l.normalized, '^duration: \? ms (?:statement|execute <unnamed>): (.*)'::text) AS statement_norm
   FROM ((sn_pglog l
     JOIN sn_stat_snapshot sss ON ((sss.snap_id = l.snap_id)))
     LEFT JOIN pm_database d ON (((d.instance_id = sss.instance_id) AND (d.name = l.database_name))))
  WHERE (((l.normalized ~ '^duration: .* ms (statement|execute).*'::text) AND (l.error_severity = 'LOG'::text)) AND (l.normalized !~ '(COMMIT|EXPLAIN)'::text));


ALTER TABLE vw_sn_pglog_statements OWNER TO pganalytics;

--
-- Name: vw_sn_stat_database; Type: VIEW; Schema: template_customer; Owner: pganalytics
--

CREATE VIEW vw_sn_stat_database AS
 WITH s AS (
         SELECT s_1.datname,
            date_trunc('hour'::text, sss.datetime) AS datetime,
            s_1.numbackends,
            s_1.temp_files,
            s_1.temp_bytes,
            s_1.deadlocks,
            (s_1.blks_read - lag(s_1.blks_read) OVER w) AS blks_read,
            (s_1.blks_hit - lag(s_1.blks_hit) OVER w) AS blks_hit,
            sss.customer_id,
            sss.server_id,
            sss.instance_id,
            sss.database_id
           FROM (sn_stat_database s_1
             JOIN sn_stat_snapshot sss USING (snap_id))
          WINDOW w AS (PARTITION BY s_1.datname ORDER BY s_1.snap_id)
          ORDER BY sss.snap_id DESC
        )
 SELECT s.datname,
    s.datetime,
    s.customer_id,
    s.server_id,
    s.instance_id,
    s.database_id,
    s.numbackends,
    s.temp_files,
    s.temp_bytes,
    s.deadlocks,
    (((s.blks_hit)::numeric / NULLIF(((s.blks_read + s.blks_hit))::numeric, (0)::numeric)) * (100)::numeric) AS cache_ratio
   FROM s;


ALTER TABLE vw_sn_stat_database OWNER TO pganalytics;

--
-- Name: database_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_database ALTER COLUMN database_id SET DEFAULT nextval('pm_database_database_id_seq'::regclass);


--
-- Name: instance_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_instance ALTER COLUMN instance_id SET DEFAULT nextval('pm_instance_seq'::regclass);


--
-- Name: server_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_server ALTER COLUMN server_id SET DEFAULT nextval('pm_server_seq'::regclass);


--
-- Name: snap_type_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_snap_type ALTER COLUMN snap_type_id SET DEFAULT nextval('pm_snap_type_snap_type_id_seq'::regclass);


--
-- Name: snap_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot ALTER COLUMN snap_id SET DEFAULT nextval('sn_stat_snapshot_seq'::regclass);


--
-- Name: statement_id; Type: DEFAULT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements ALTER COLUMN statement_id SET DEFAULT nextval('sn_statements_statement_id_seq'::regclass);


--
-- Name: mv_sn_sysstat_disks_devs_pkey; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY mv_sn_sysstat_disks_devs
    ADD CONSTRAINT mv_sn_sysstat_disks_devs_pkey PRIMARY KEY (server_id, dev);


--
-- Name: pm_database_pk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY pm_database
    ADD CONSTRAINT pm_database_pk PRIMARY KEY (database_id);


--
-- Name: pm_instance_pk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY pm_instance
    ADD CONSTRAINT pm_instance_pk PRIMARY KEY (instance_id);


--
-- Name: pm_instance_serverport_uk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY pm_instance
    ADD CONSTRAINT pm_instance_serverport_uk UNIQUE (server_id, port);


--
-- Name: pm_server_pk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY pm_server
    ADD CONSTRAINT pm_server_pk PRIMARY KEY (server_id);


--
-- Name: pm_snap_type_pkey; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY pm_snap_type
    ADD CONSTRAINT pm_snap_type_pkey PRIMARY KEY (snap_type_id);


--
-- Name: sn_data_info_pkey; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY sn_data_info
    ADD CONSTRAINT sn_data_info_pkey PRIMARY KEY (snap_id, data_key);


--
-- Name: sn_stat_snapshot_hash_uk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT sn_stat_snapshot_hash_uk UNIQUE (server_id, instance_id, database_id, snap_hash);


--
-- Name: sn_stat_snapshot_pk; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT sn_stat_snapshot_pk PRIMARY KEY (snap_id);


--
-- Name: sn_statements_pkey; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY sn_statements
    ADD CONSTRAINT sn_statements_pkey PRIMARY KEY (statement_id);


--
-- Name: sn_statements_statement_md5_key; Type: CONSTRAINT; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

ALTER TABLE ONLY sn_statements
    ADD CONSTRAINT sn_statements_statement_md5_key UNIQUE (statement_md5);


--
-- Name: mvw_pglog_autovacuum_row_number; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE UNIQUE INDEX mvw_pglog_autovacuum_row_number ON mvw_pglog_autovacuum USING btree (row_number);


--
-- Name: mvw_pglog_checkpoint_row_number; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE UNIQUE INDEX mvw_pglog_checkpoint_row_number ON mvw_pglog_checkpoint USING btree (row_number);


--
-- Name: sn_data_info_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_data_info_snap_id_idx ON sn_data_info USING btree (snap_id);


--
-- Name: sn_database_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_database_snap_id_idx ON sn_database USING btree (snap_id);


--
-- Name: sn_diagnostic_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_diagnostic_snap_id_idx ON sn_diagnostic USING btree (snap_id);


--
-- Name: sn_disk_usage_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_disk_usage_snap_id_idx ON sn_disk_usage USING btree (snap_id);


--
-- Name: sn_instance_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_instance_snap_id_idx ON sn_instance USING btree (snap_id);


--
-- Name: sn_namespace_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_namespace_snap_id_idx ON sn_namespace USING btree (snap_id);


--
-- Name: sn_pglog_log_time_error_severity_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_pglog_log_time_error_severity_idx ON sn_pglog USING btree (log_time, error_severity);


--
-- Name: sn_pglog_log_time_pid_session_line_num_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_pglog_log_time_pid_session_line_num_idx ON sn_pglog USING btree (log_time, pid, session_line_num);


--
-- Name: sn_pglog_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_pglog_snap_id_idx ON sn_pglog USING btree (snap_id);


--
-- Name: sn_pglog_snap_id_idx1; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_pglog_snap_id_idx1 ON sn_pglog USING btree (snap_id) WHERE (((normalized ~ '^duration: .* ms (statement|execute|parse|bind).*:'::text) AND (error_severity = 'LOG'::text)) AND (normalized !~ '(COMMIT|EXPLAIN|COPY)'::text));


--
-- Name: sn_relations_relsize_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_relations_relsize_idx ON sn_relations USING btree (relsize);


--
-- Name: sn_relations_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_relations_snap_id_idx ON sn_relations USING btree (snap_id);


--
-- Name: sn_relations_snap_id_relname_nspname_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_relations_snap_id_relname_nspname_idx ON sn_relations USING btree (snap_id, relname, nspname);


--
-- Name: sn_settings_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_settings_snap_id_idx ON sn_settings USING btree (snap_id);


--
-- Name: sn_stat_archiver_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_archiver_snap_id_idx ON sn_stat_archiver USING btree (snap_id);


--
-- Name: sn_stat_bgwriter_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_bgwriter_snap_id_idx ON sn_stat_bgwriter USING btree (snap_id);


--
-- Name: sn_stat_database_conflicts_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_database_conflicts_snap_id_idx ON sn_stat_database_conflicts USING btree (snap_id);


--
-- Name: sn_stat_database_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_database_snap_id_idx ON sn_stat_database USING btree (snap_id);


--
-- Name: sn_stat_replication_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_replication_snap_id_idx ON sn_stat_replication USING btree (snap_id);


--
-- Name: sn_stat_replication_snap_id_idx1; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_replication_snap_id_idx1 ON sn_stat_replication USING btree (snap_id);


--
-- Name: sn_stat_snapshot_customer_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_snapshot_customer_id_idx ON sn_stat_snapshot USING btree (customer_id);


--
-- Name: sn_stat_snapshot_database_id_datetime_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_snapshot_database_id_datetime_idx ON sn_stat_snapshot USING btree (database_id, datetime);


--
-- Name: sn_stat_snapshot_database_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_snapshot_database_id_idx ON sn_stat_snapshot USING btree (database_id);


--
-- Name: sn_stat_snapshot_datetime_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_snapshot_datetime_idx ON sn_stat_snapshot USING btree (datetime);


--
-- Name: sn_stat_snapshot_snap_type_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_snapshot_snap_type_idx ON sn_stat_snapshot USING btree (snap_type);


--
-- Name: sn_stat_user_functions_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_functions_snap_id_idx ON sn_stat_user_functions USING btree (snap_id);


--
-- Name: sn_stat_user_indexes_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_indexes_snap_id_idx ON sn_stat_user_indexes USING btree (snap_id);


--
-- Name: sn_stat_user_tables_autoanalyze_count_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_autoanalyze_count_idx ON sn_stat_user_tables USING btree (autoanalyze_count);


--
-- Name: sn_stat_user_tables_autovacuum_count_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_autovacuum_count_idx ON sn_stat_user_tables USING btree (autovacuum_count);


--
-- Name: sn_stat_user_tables_n_dead_tup_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_n_dead_tup_idx ON sn_stat_user_tables USING btree (n_dead_tup);


--
-- Name: sn_stat_user_tables_n_live_tup_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_n_live_tup_idx ON sn_stat_user_tables USING btree (n_live_tup);


--
-- Name: sn_stat_user_tables_relname_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_relname_idx ON sn_stat_user_tables USING btree (relname);


--
-- Name: sn_stat_user_tables_schemaname_relname_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_schemaname_relname_snap_id_idx ON sn_stat_user_tables USING btree (schemaname, relname, snap_id);


--
-- Name: sn_stat_user_tables_seq_scan_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_seq_scan_idx ON sn_stat_user_tables USING btree (seq_scan);


--
-- Name: sn_stat_user_tables_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_snap_id_idx ON sn_stat_user_tables USING btree (snap_id);


--
-- Name: sn_stat_user_tables_snap_id_relname_schemaname_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stat_user_tables_snap_id_relname_schemaname_idx ON sn_stat_user_tables USING btree (snap_id, relname, schemaname);


--
-- Name: sn_statements_executed_server_id_instance_id_log_time_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_statements_executed_server_id_instance_id_log_time_idx ON sn_statements_executed USING btree (server_id, instance_id, log_time);


--
-- Name: sn_statements_executed_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_statements_executed_snap_id_idx ON sn_statements_executed USING btree (snap_id);


--
-- Name: sn_statio_user_indexes_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_statio_user_indexes_snap_id_idx ON sn_statio_user_indexes USING btree (snap_id);


--
-- Name: sn_statio_user_sequences_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_statio_user_sequences_snap_id_idx ON sn_statio_user_sequences USING btree (snap_id);


--
-- Name: sn_statio_user_tables_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_statio_user_tables_snap_id_idx ON sn_statio_user_tables USING btree (snap_id);


--
-- Name: sn_stats_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_stats_snap_id_idx ON sn_stats USING btree (snap_id);


--
-- Name: sn_sysstat_cpu_cpu_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_cpu_cpu_idx ON sn_sysstat_cpu USING btree (cpu);


--
-- Name: sn_sysstat_cpu_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_cpu_snap_id_idx ON sn_sysstat_cpu USING btree (snap_id);


--
-- Name: sn_sysstat_disks_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_disks_snap_id_idx ON sn_sysstat_disks USING btree (snap_id);


--
-- Name: sn_sysstat_hugepages_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_hugepages_snap_id_idx ON sn_sysstat_hugepages USING btree (snap_id);


--
-- Name: sn_sysstat_io_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_io_snap_id_idx ON sn_sysstat_io USING btree (snap_id);


--
-- Name: sn_sysstat_kerneltables_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_kerneltables_snap_id_idx ON sn_sysstat_kerneltables USING btree (snap_id);


--
-- Name: sn_sysstat_loadqueue_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_loadqueue_snap_id_idx ON sn_sysstat_loadqueue USING btree (snap_id);


--
-- Name: sn_sysstat_memstats_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_memstats_snap_id_idx ON sn_sysstat_memstats USING btree (snap_id);


--
-- Name: sn_sysstat_memusage_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_memusage_snap_id_idx ON sn_sysstat_memusage USING btree (snap_id);


--
-- Name: sn_sysstat_network_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_network_snap_id_idx ON sn_sysstat_network USING btree (snap_id);


--
-- Name: sn_sysstat_paging_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_paging_snap_id_idx ON sn_sysstat_paging USING btree (snap_id);


--
-- Name: sn_sysstat_swapstats_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_swapstats_snap_id_idx ON sn_sysstat_swapstats USING btree (snap_id);


--
-- Name: sn_sysstat_swapusage_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_swapusage_snap_id_idx ON sn_sysstat_swapusage USING btree (snap_id);


--
-- Name: sn_sysstat_tasks_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_sysstat_tasks_snap_id_idx ON sn_sysstat_tasks USING btree (snap_id);


--
-- Name: sn_tablespace_snap_id_idx; Type: INDEX; Schema: template_customer; Owner: pganalytics; Tablespace: 
--

CREATE INDEX sn_tablespace_snap_id_idx ON sn_tablespace USING btree (snap_id);


--
-- Name: tg_make_cols_null; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_make_cols_null BEFORE INSERT ON sn_stat_user_indexes FOR EACH ROW EXECUTE PROCEDURE pganalytics.tg_make_cols_null('schemaname', 'relname', 'indexrelname');


--
-- Name: tg_make_cols_null; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_make_cols_null BEFORE INSERT ON sn_stat_user_tables FOR EACH ROW EXECUTE PROCEDURE pganalytics.tg_make_cols_null('schemaname', 'relname');


--
-- Name: tg_make_cols_null; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_make_cols_null BEFORE INSERT ON sn_statio_user_indexes FOR EACH ROW EXECUTE PROCEDURE pganalytics.tg_make_cols_null('schemaname', 'relname', 'indexrelname');


--
-- Name: tg_make_cols_null; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_make_cols_null BEFORE INSERT ON sn_statio_user_tables FOR EACH ROW EXECUTE PROCEDURE pganalytics.tg_make_cols_null('schemaname', 'relname');


--
-- Name: tg_make_cols_null; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_make_cols_null BEFORE INSERT ON sn_statio_user_sequences FOR EACH ROW EXECUTE PROCEDURE pganalytics.tg_make_cols_null('schemaname', 'relname');


--
-- Name: tg_pm_pglog_ignore; Type: TRIGGER; Schema: template_customer; Owner: pganalytics
--

CREATE TRIGGER tg_pm_pglog_ignore BEFORE INSERT ON sn_pglog FOR EACH ROW EXECUTE PROCEDURE tg_pm_pglog_ignore();


--
-- Name: fk_pmdatabase_pminstance; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_database
    ADD CONSTRAINT fk_pmdatabase_pminstance FOREIGN KEY (instance_id) REFERENCES pm_instance(instance_id) ON UPDATE CASCADE DEFERRABLE;


--
-- Name: fk_pminstance_pmserver; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_instance
    ADD CONSTRAINT fk_pminstance_pmserver FOREIGN KEY (server_id) REFERENCES pm_server(server_id) ON UPDATE CASCADE DEFERRABLE;


--
-- Name: fk_pmserver_pmcustomer; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_server
    ADD CONSTRAINT fk_pmserver_pmcustomer FOREIGN KEY (customer_id) REFERENCES pganalytics.pm_customer(customer_id) ON UPDATE CASCADE DEFERRABLE;


--
-- Name: fk_snapshot_snaptype; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT fk_snapshot_snaptype FOREIGN KEY (snap_type_id) REFERENCES pm_snap_type(snap_type_id);


--
-- Name: fk_sntatsnapshot_pmcustomer; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT fk_sntatsnapshot_pmcustomer FOREIGN KEY (customer_id) REFERENCES pganalytics.pm_customer(customer_id) ON UPDATE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: fk_sntatsnapshot_pmdatabase; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT fk_sntatsnapshot_pmdatabase FOREIGN KEY (database_id) REFERENCES pm_database(database_id) ON UPDATE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: fk_sntatsnapshot_pminstance; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT fk_sntatsnapshot_pminstance FOREIGN KEY (instance_id) REFERENCES pm_instance(instance_id) ON UPDATE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: fk_sntatsnapshot_pmserver; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_snapshot
    ADD CONSTRAINT fk_sntatsnapshot_pmserver FOREIGN KEY (server_id) REFERENCES pm_server(server_id) ON UPDATE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: mv_sn_sysstat_disks_devs_server_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY mv_sn_sysstat_disks_devs
    ADD CONSTRAINT mv_sn_sysstat_disks_devs_server_id_fkey FOREIGN KEY (server_id) REFERENCES pm_server(server_id);


--
-- Name: pm_instance_master_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY pm_instance
    ADD CONSTRAINT pm_instance_master_id_fkey FOREIGN KEY (master_id) REFERENCES pm_instance(instance_id);


--
-- Name: sn_data_info_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_data_info
    ADD CONSTRAINT sn_data_info_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_database_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_database
    ADD CONSTRAINT sn_database_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_diagnostic_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_diagnostic
    ADD CONSTRAINT sn_diagnostic_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_disk_usage_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_disk_usage
    ADD CONSTRAINT sn_disk_usage_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_instance_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_instance
    ADD CONSTRAINT sn_instance_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_namespace_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_namespace
    ADD CONSTRAINT sn_namespace_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_pglog_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_pglog
    ADD CONSTRAINT sn_pglog_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_relations_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_relations
    ADD CONSTRAINT sn_relations_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_settings_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_settings
    ADD CONSTRAINT sn_settings_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_archiver_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_archiver
    ADD CONSTRAINT sn_stat_archiver_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_bgwriter_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_bgwriter
    ADD CONSTRAINT sn_stat_bgwriter_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_database_conflicts_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_database_conflicts
    ADD CONSTRAINT sn_stat_database_conflicts_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_database_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_database
    ADD CONSTRAINT sn_stat_database_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_replication_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_replication
    ADD CONSTRAINT sn_stat_replication_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_user_functions_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_user_functions
    ADD CONSTRAINT sn_stat_user_functions_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_user_indexes_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_user_indexes
    ADD CONSTRAINT sn_stat_user_indexes_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stat_user_tables_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stat_user_tables
    ADD CONSTRAINT sn_stat_user_tables_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_statements_executed_database_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements_executed
    ADD CONSTRAINT sn_statements_executed_database_id_fkey FOREIGN KEY (database_id) REFERENCES pm_database(database_id) ON UPDATE CASCADE ON DELETE CASCADE;


--
-- Name: sn_statements_executed_instance_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements_executed
    ADD CONSTRAINT sn_statements_executed_instance_id_fkey FOREIGN KEY (instance_id) REFERENCES pm_instance(instance_id) ON UPDATE CASCADE ON DELETE CASCADE;


--
-- Name: sn_statements_executed_server_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements_executed
    ADD CONSTRAINT sn_statements_executed_server_id_fkey FOREIGN KEY (server_id) REFERENCES pm_server(server_id) ON UPDATE CASCADE ON DELETE CASCADE;


--
-- Name: sn_statements_executed_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements_executed
    ADD CONSTRAINT sn_statements_executed_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE;


--
-- Name: sn_statements_executed_statement_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statements_executed
    ADD CONSTRAINT sn_statements_executed_statement_id_fkey FOREIGN KEY (statement_id) REFERENCES sn_statements(statement_id);


--
-- Name: sn_statio_user_indexes_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statio_user_indexes
    ADD CONSTRAINT sn_statio_user_indexes_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_statio_user_sequences_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statio_user_sequences
    ADD CONSTRAINT sn_statio_user_sequences_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_statio_user_tables_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_statio_user_tables
    ADD CONSTRAINT sn_statio_user_tables_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_stats_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_stats
    ADD CONSTRAINT sn_stats_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_cpu_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_cpu
    ADD CONSTRAINT sn_sysstat_cpu_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_disks_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_disks
    ADD CONSTRAINT sn_sysstat_disks_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_hugepages_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_hugepages
    ADD CONSTRAINT sn_sysstat_hugepages_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_io_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_io
    ADD CONSTRAINT sn_sysstat_io_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_kerneltables_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_kerneltables
    ADD CONSTRAINT sn_sysstat_kerneltables_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_loadqueue_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_loadqueue
    ADD CONSTRAINT sn_sysstat_loadqueue_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_memstats_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_memstats
    ADD CONSTRAINT sn_sysstat_memstats_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_memusage_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_memusage
    ADD CONSTRAINT sn_sysstat_memusage_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_network_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_network
    ADD CONSTRAINT sn_sysstat_network_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_paging_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_paging
    ADD CONSTRAINT sn_sysstat_paging_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_swapstats_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_swapstats
    ADD CONSTRAINT sn_sysstat_swapstats_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_swapusage_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_swapusage
    ADD CONSTRAINT sn_sysstat_swapusage_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_sysstat_tasks_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_sysstat_tasks
    ADD CONSTRAINT sn_sysstat_tasks_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: sn_tablespace_snap_id_fkey; Type: FK CONSTRAINT; Schema: template_customer; Owner: pganalytics
--

ALTER TABLE ONLY sn_tablespace
    ADD CONSTRAINT sn_tablespace_snap_id_fkey FOREIGN KEY (snap_id) REFERENCES sn_stat_snapshot(snap_id) ON UPDATE CASCADE ON DELETE CASCADE DEFERRABLE INITIALLY DEFERRED;


--
-- Name: template_customer; Type: ACL; Schema: -; Owner: pganalytics
--

REVOKE ALL ON SCHEMA template_customer FROM PUBLIC;
REVOKE ALL ON SCHEMA template_customer FROM pganalytics;
GRANT ALL ON SCHEMA template_customer TO pganalytics;
GRANT USAGE ON SCHEMA template_customer TO email_sender;


--
-- Name: log_error(text, text, text, text, text, integer); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION log_error(customer text, action text, errcode text, message text, source text, line integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION log_error(customer text, action text, errcode text, message text, source text, line integer) FROM pganalytics;
GRANT ALL ON FUNCTION log_error(customer text, action text, errcode text, message text, source text, line integer) TO pganalytics;
GRANT ALL ON FUNCTION log_error(customer text, action text, errcode text, message text, source text, line integer) TO PUBLIC;


--
-- Name: pm_customer_get_current(); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION pm_customer_get_current() FROM PUBLIC;
REVOKE ALL ON FUNCTION pm_customer_get_current() FROM pganalytics;
GRANT ALL ON FUNCTION pm_customer_get_current() TO pganalytics;
GRANT ALL ON FUNCTION pm_customer_get_current() TO PUBLIC;


--
-- Name: pm_database_get_id(name, integer); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION pm_database_get_id(p_datname name, p_instance_id integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION pm_database_get_id(p_datname name, p_instance_id integer) FROM pganalytics;
GRANT ALL ON FUNCTION pm_database_get_id(p_datname name, p_instance_id integer) TO pganalytics;
GRANT ALL ON FUNCTION pm_database_get_id(p_datname name, p_instance_id integer) TO PUBLIC;


--
-- Name: sn_import_snapshot(snap_type_enum, text, text, timestamp with time zone, timestamp with time zone, text, text, text); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) FROM PUBLIC;
REVOKE ALL ON FUNCTION sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) FROM pganalytics;
GRANT ALL ON FUNCTION sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) TO pganalytics;
GRANT ALL ON FUNCTION sn_import_snapshot(snap_type snap_type_enum, customer_name text, server_name text, datetime timestamp with time zone, real_datetime timestamp with time zone, instance_name text, datname text, snap_hash text) TO PUBLIC;


--
-- Name: sn_sysstat_import(integer, integer); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION sn_sysstat_import(p_customer_id integer, p_server_id integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION sn_sysstat_import(p_customer_id integer, p_server_id integer) FROM pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import(p_customer_id integer, p_server_id integer) TO pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import(p_customer_id integer, p_server_id integer) TO PUBLIC;


--
-- Name: sn_sysstat_import(text, text); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION sn_sysstat_import(p_customer_name text, p_server_name text) FROM PUBLIC;
REVOKE ALL ON FUNCTION sn_sysstat_import(p_customer_name text, p_server_name text) FROM pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import(p_customer_name text, p_server_name text) TO pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import(p_customer_name text, p_server_name text) TO PUBLIC;


--
-- Name: sn_sysstat_import_refresh_mvs(integer, integer); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) FROM PUBLIC;
REVOKE ALL ON FUNCTION sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) FROM pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) TO pganalytics;
GRANT ALL ON FUNCTION sn_sysstat_import_refresh_mvs(p_customer_id integer, p_server_id integer) TO PUBLIC;


--
-- Name: sn_tablespace_update_fsdevice(name, text); Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON FUNCTION sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) FROM PUBLIC;
REVOKE ALL ON FUNCTION sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) FROM pganalytics;
GRANT ALL ON FUNCTION sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) TO pganalytics;
GRANT ALL ON FUNCTION sn_tablespace_update_fsdevice(p_spcname name, p_fsdevice text) TO PUBLIC;


--
-- Name: mv_sn_sysstat_disks_devs; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE mv_sn_sysstat_disks_devs FROM PUBLIC;
REVOKE ALL ON TABLE mv_sn_sysstat_disks_devs FROM pganalytics;
GRANT ALL ON TABLE mv_sn_sysstat_disks_devs TO pganalytics;


--
-- Name: sn_stat_snapshot; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_snapshot FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_snapshot FROM pganalytics;
GRANT ALL ON TABLE sn_stat_snapshot TO pganalytics;


--
-- Name: sn_stat_snapshot_seq; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON SEQUENCE sn_stat_snapshot_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE sn_stat_snapshot_seq FROM pganalytics;
GRANT ALL ON SEQUENCE sn_stat_snapshot_seq TO pganalytics;


--
-- Name: sn_pglog; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_pglog FROM PUBLIC;
REVOKE ALL ON TABLE sn_pglog FROM pganalytics;
GRANT ALL ON TABLE sn_pglog TO pganalytics;


--
-- Name: vw_sn_pglog_autovacuum; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_pglog_autovacuum FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_pglog_autovacuum FROM pganalytics;
GRANT ALL ON TABLE vw_sn_pglog_autovacuum TO pganalytics;


--
-- Name: vw_sn_pglog_checkpoints; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_pglog_checkpoints FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_pglog_checkpoints FROM pganalytics;
GRANT ALL ON TABLE vw_sn_pglog_checkpoints TO pganalytics;


--
-- Name: pm_database; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE pm_database FROM PUBLIC;
REVOKE ALL ON TABLE pm_database FROM pganalytics;
GRANT ALL ON TABLE pm_database TO pganalytics;


--
-- Name: pm_database_database_id_seq; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON SEQUENCE pm_database_database_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE pm_database_database_id_seq FROM pganalytics;
GRANT ALL ON SEQUENCE pm_database_database_id_seq TO pganalytics;


--
-- Name: pm_instance; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE pm_instance FROM PUBLIC;
REVOKE ALL ON TABLE pm_instance FROM pganalytics;
GRANT ALL ON TABLE pm_instance TO pganalytics;


--
-- Name: pm_instance_seq; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON SEQUENCE pm_instance_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE pm_instance_seq FROM pganalytics;
GRANT ALL ON SEQUENCE pm_instance_seq TO pganalytics;


--
-- Name: pm_server; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE pm_server FROM PUBLIC;
REVOKE ALL ON TABLE pm_server FROM pganalytics;
GRANT ALL ON TABLE pm_server TO pganalytics;
GRANT SELECT ON TABLE pm_server TO email_sender;


--
-- Name: pm_server_seq; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON SEQUENCE pm_server_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE pm_server_seq FROM pganalytics;
GRANT ALL ON SEQUENCE pm_server_seq TO pganalytics;


--
-- Name: pm_snap_type; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE pm_snap_type FROM PUBLIC;
REVOKE ALL ON TABLE pm_snap_type FROM pganalytics;
GRANT ALL ON TABLE pm_snap_type TO pganalytics;


--
-- Name: pm_snap_type_snap_type_id_seq; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON SEQUENCE pm_snap_type_snap_type_id_seq FROM PUBLIC;
REVOKE ALL ON SEQUENCE pm_snap_type_snap_type_id_seq FROM pganalytics;
GRANT ALL ON SEQUENCE pm_snap_type_snap_type_id_seq TO pganalytics;


--
-- Name: sn_data_info; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_data_info FROM PUBLIC;
REVOKE ALL ON TABLE sn_data_info FROM pganalytics;
GRANT ALL ON TABLE sn_data_info TO pganalytics;


--
-- Name: sn_database; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_database FROM PUBLIC;
REVOKE ALL ON TABLE sn_database FROM pganalytics;
GRANT ALL ON TABLE sn_database TO pganalytics;


--
-- Name: sn_diagnostic; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_diagnostic FROM PUBLIC;
REVOKE ALL ON TABLE sn_diagnostic FROM pganalytics;
GRANT ALL ON TABLE sn_diagnostic TO pganalytics;


--
-- Name: sn_disk_usage; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_disk_usage FROM PUBLIC;
REVOKE ALL ON TABLE sn_disk_usage FROM pganalytics;
GRANT ALL ON TABLE sn_disk_usage TO pganalytics;


--
-- Name: sn_instance; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_instance FROM PUBLIC;
REVOKE ALL ON TABLE sn_instance FROM pganalytics;
GRANT ALL ON TABLE sn_instance TO pganalytics;


--
-- Name: sn_namespace; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_namespace FROM PUBLIC;
REVOKE ALL ON TABLE sn_namespace FROM pganalytics;
GRANT ALL ON TABLE sn_namespace TO pganalytics;


--
-- Name: sn_relations; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_relations FROM PUBLIC;
REVOKE ALL ON TABLE sn_relations FROM pganalytics;
GRANT ALL ON TABLE sn_relations TO pganalytics;


--
-- Name: sn_stat_bgwriter; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_bgwriter FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_bgwriter FROM pganalytics;
GRANT ALL ON TABLE sn_stat_bgwriter TO pganalytics;


--
-- Name: sn_stat_database; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_database FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_database FROM pganalytics;
GRANT ALL ON TABLE sn_stat_database TO pganalytics;


--
-- Name: sn_stat_user_functions; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_user_functions FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_user_functions FROM pganalytics;
GRANT ALL ON TABLE sn_stat_user_functions TO pganalytics;


--
-- Name: sn_stat_user_indexes; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_user_indexes FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_user_indexes FROM pganalytics;
GRANT ALL ON TABLE sn_stat_user_indexes TO pganalytics;


--
-- Name: sn_stat_user_tables; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stat_user_tables FROM PUBLIC;
REVOKE ALL ON TABLE sn_stat_user_tables FROM pganalytics;
GRANT ALL ON TABLE sn_stat_user_tables TO pganalytics;


--
-- Name: sn_statio_user_indexes; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_statio_user_indexes FROM PUBLIC;
REVOKE ALL ON TABLE sn_statio_user_indexes FROM pganalytics;
GRANT ALL ON TABLE sn_statio_user_indexes TO pganalytics;


--
-- Name: sn_statio_user_sequences; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_statio_user_sequences FROM PUBLIC;
REVOKE ALL ON TABLE sn_statio_user_sequences FROM pganalytics;
GRANT ALL ON TABLE sn_statio_user_sequences TO pganalytics;


--
-- Name: sn_statio_user_tables; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_statio_user_tables FROM PUBLIC;
REVOKE ALL ON TABLE sn_statio_user_tables FROM pganalytics;
GRANT ALL ON TABLE sn_statio_user_tables TO pganalytics;


--
-- Name: sn_stats; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_stats FROM PUBLIC;
REVOKE ALL ON TABLE sn_stats FROM pganalytics;
GRANT ALL ON TABLE sn_stats TO pganalytics;


--
-- Name: sn_sysstat_cpu; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_cpu FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_cpu FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_cpu TO pganalytics;


--
-- Name: sn_sysstat_disks; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_disks FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_disks FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_disks TO pganalytics;


--
-- Name: sn_sysstat_hugepages; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_hugepages FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_hugepages FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_hugepages TO pganalytics;


--
-- Name: sn_sysstat_io; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_io FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_io FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_io TO pganalytics;


--
-- Name: sn_sysstat_kerneltables; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_kerneltables FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_kerneltables FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_kerneltables TO pganalytics;


--
-- Name: sn_sysstat_loadqueue; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_loadqueue FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_loadqueue FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_loadqueue TO pganalytics;


--
-- Name: sn_sysstat_memstats; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_memstats FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_memstats FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_memstats TO pganalytics;


--
-- Name: sn_sysstat_memusage; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_memusage FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_memusage FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_memusage TO pganalytics;


--
-- Name: sn_sysstat_network; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_network FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_network FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_network TO pganalytics;


--
-- Name: sn_sysstat_paging; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_paging FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_paging FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_paging TO pganalytics;


--
-- Name: sn_sysstat_swapstats; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_swapstats FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_swapstats FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_swapstats TO pganalytics;


--
-- Name: sn_sysstat_swapusage; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_swapusage FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_swapusage FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_swapusage TO pganalytics;


--
-- Name: sn_sysstat_tasks; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_sysstat_tasks FROM PUBLIC;
REVOKE ALL ON TABLE sn_sysstat_tasks FROM pganalytics;
GRANT ALL ON TABLE sn_sysstat_tasks TO pganalytics;


--
-- Name: sn_tablespace; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE sn_tablespace FROM PUBLIC;
REVOKE ALL ON TABLE sn_tablespace FROM pganalytics;
GRANT ALL ON TABLE sn_tablespace TO pganalytics;


--
-- Name: vw_backup_list; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_backup_list FROM PUBLIC;
REVOKE ALL ON TABLE vw_backup_list FROM pganalytics;
GRANT ALL ON TABLE vw_backup_list TO pganalytics;


--
-- Name: vw_sn_last_snapshots; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_last_snapshots FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_last_snapshots FROM pganalytics;
GRANT ALL ON TABLE vw_sn_last_snapshots TO pganalytics;


--
-- Name: vw_sn_last_stat_snapshot; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_last_stat_snapshot FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_last_stat_snapshot FROM pganalytics;
GRANT ALL ON TABLE vw_sn_last_stat_snapshot TO pganalytics;


--
-- Name: vw_sn_pglog_statements; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_pglog_statements FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_pglog_statements FROM pganalytics;
GRANT ALL ON TABLE vw_sn_pglog_statements TO pganalytics;


--
-- Name: vw_sn_stat_database; Type: ACL; Schema: template_customer; Owner: pganalytics
--

REVOKE ALL ON TABLE vw_sn_stat_database FROM PUBLIC;
REVOKE ALL ON TABLE vw_sn_stat_database FROM pganalytics;
GRANT ALL ON TABLE vw_sn_stat_database TO pganalytics;


--
-- PostgreSQL database dump complete
--

