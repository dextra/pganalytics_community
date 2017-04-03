--
-- PostgreSQL database cluster dump
--

SET default_transaction_read_only = off;

SET client_encoding = 'UTF8';
SET standard_conforming_strings = on;

--
-- Roles
--

CREATE ROLE ctm;
ALTER ROLE ctm WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB NOLOGIN NOREPLICATION;
CREATE ROLE ctm_demo;
ALTER ROLE ctm_demo WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE ctm_demo_imp;
ALTER ROLE ctm_demo_imp WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
CREATE ROLE ctm_demo_ro;
ALTER ROLE ctm_demo_ro WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
CREATE ROLE ctm_importer;
ALTER ROLE ctm_importer WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE ctm_pganalytics;
ALTER ROLE ctm_pganalytics WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE ctm_pganalytics_imp;
ALTER ROLE ctm_pganalytics_imp WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE ctm_pganalytics_ro;
ALTER ROLE ctm_pganalytics_ro WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE email_sender;
ALTER ROLE email_sender WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
CREATE ROLE pga_app;
ALTER ROLE pga_app WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE pga_app_master;
ALTER ROLE pga_app_master WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE pga_app_public;
ALTER ROLE pga_app_public WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION;
CREATE ROLE pga_importer;
ALTER ROLE pga_importer WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
CREATE ROLE pga_ro;
ALTER ROLE pga_ro WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
CREATE ROLE pganalytics;
ALTER ROLE pganalytics WITH NOSUPERUSER INHERIT NOCREATEROLE NOCREATEDB LOGIN NOREPLICATION ;
ALTER ROLE ctm_demo_imp SET search_path TO ctm_demo;
ALTER ROLE ctm_demo_ro SET search_path TO ctm_demo, public;
ALTER ROLE ctm_importer SET log_min_duration_statement TO '-1';
ALTER ROLE ctm_pganalytics_imp SET search_path TO ctm_pganalytics, public;
ALTER ROLE ctm_pganalytics_ro SET search_path TO ctm_pganalytics, public;
ALTER ROLE email_sender SET log_min_duration_statement TO '-1';
ALTER ROLE pga_app_master SET search_path TO pganalytics, public;
ALTER ROLE pga_app_public SET search_path TO pganalytics, public;
ALTER ROLE pganalytics SET search_path TO pganalytics, public;


--
-- Role memberships
--

GRANT ctm TO ctm_demo GRANTED BY postgres;
GRANT ctm TO ctm_importer GRANTED BY postgres;
GRANT ctm TO ctm_pganalytics GRANTED BY postgres;
GRANT ctm_demo TO ctm_demo_imp GRANTED BY postgres;
GRANT ctm_demo TO ctm_demo_ro GRANTED BY postgres;
GRANT ctm_pganalytics TO ctm_pganalytics_imp GRANTED BY postgres;
GRANT ctm_pganalytics TO ctm_pganalytics_ro GRANTED BY postgres;
GRANT ctm_pganalytics_imp TO ctm_importer GRANTED BY postgres;
GRANT pga_app TO pga_app_master GRANTED BY postgres;
GRANT pga_app TO pga_app_public GRANTED BY postgres;


--
-- Database creation
--

CREATE DATABASE pganalytics WITH TEMPLATE = template0 OWNER = pganalytics;
REVOKE ALL ON DATABASE pganalytics FROM PUBLIC;
REVOKE ALL ON DATABASE pganalytics FROM pganalytics;
GRANT ALL ON DATABASE pganalytics TO pganalytics;
GRANT CONNECT,TEMPORARY ON DATABASE pganalytics TO PUBLIC;
REVOKE ALL ON DATABASE template1 FROM PUBLIC;
REVOKE ALL ON DATABASE template1 FROM postgres;
GRANT ALL ON DATABASE template1 TO postgres;
GRANT CONNECT ON DATABASE template1 TO PUBLIC;


--
-- PostgreSQL database cluster dump complete
--

