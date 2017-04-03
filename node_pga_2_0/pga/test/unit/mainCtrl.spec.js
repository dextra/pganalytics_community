'use strict';

describe('MainCtrl', function() {
	var createMainCtrl, scope;
	beforeEach(function() { module('pganalytics'); });
	beforeEach(inject(function($controller, $rootScope, $httpBackend) {
		createMainCtrl = function() {
			scope = $rootScope.$new();
			return $controller('MainCtrl', {'$scope': scope});
		}
	}));
	it('should get default section and title', function() {
		inject(function($controller, $rootScope, $httpBackend) {

			$httpBackend
				.expect('GET', '/pga/s/sql/public/i18n_getall?f=expanded&p=%7B%22language%22%3A%22pt_BR%22%7D')
				.respond(200, [{"data":{}}]);

			$httpBackend
				.expect('GET', '/pga/s/sql/public/list_charts?')
				.respond(200,
					{"meta":[{"name":"key","type":"text"},{"name":"section","type":"text"},{"name":"extra_param","type":"text"},{"name":"type","type":"text"}],
						"rows":[
							["overview_alerts","overview","{\n\t\"change_section\": {\n\t\t\"section\": \"overview.details\",\n\t\t\"params\": [\n\t\t\t\"server_name\"\n\t\t]\n\t}\n}","dataTable"],
							["overview_alerts_by_server","overview.details",null,"dataTable"],
							["pgstats_globalcacheratio","pgstats","{\n\t\"unit\": \"mb\"\n}","stackedAreaChart"],
							["pgstats_checkpoints","pgstats.checkpoints","{\n\t\"series\": {\n\t\t\"checkpoint_by\": {\n\t\t\t\"unit\": \"int\",\n\t\t\t\"keys\": [\n\t\t\t\t\"checkpoint_req\",\n\t\t\t\t\"checkpoint_timed\"\n\t\t\t]\n\t\t},\n\t\t\"checkpoint_time\": {\n\t\t\t\"unit\": \"ms\",\n\t\t\t\"keys\": [\n\t\t\t\t\"checkpoint_write_time_ms\",\n\t\t\t\t\"checkpoint_sync_time_ms\"\n\t\t\t]\n\t\t}\n\t}\n}","stackedAreaChart"],
							["pgstats_ovwdbinfo","pgstats.databases","{\n\t\"change_section\": {\n\t\t\"section\": \"pgstats.databases.details\",\n\t\t\"params\": [\n\t\t\t\"database_name\"\n\t\t],\n\t\t\"target\": \"pgstats_dbcacheratio\"\n\t}\n}","dataTable"],
							["pgstats_databasesize","pgstats.databases.details","{\n\t\"unit\": \"mb\"\n}","stackedAreaChart"],
							["pgstats_ovwtblinfo","pgstats.tables","{\n\t\"change_section\": {\n\t\t\"section\": \"pgstats.tables.details\",\n\t\t\"params\": [\n\t\t\t\"database_name\",\n\t\t\t\"schema_name\",\n\t\t\t\"table_name\"\n\t\t],\n\t\t\"target\": \"pgstats_tablesize\"\n\t},\n\t\"server_side_process\": true\n}","dataTable"],
							["pgstats_tablesize","pgstats.tables.details","{\n\t\"unit\": \"mb\"\n}","lineChart"],
							["pglog_duration_table","sql","{\n\t\"subType\": \"sql\"\n}","dataTable"]
						]});

			$httpBackend
				.expect('GET', '/pga/s/sql/public/list_customer_schema?f=expanded')
				.respond(200, [{"customer_id":7,"schema":"ctm_demo","name_id":"demo","name":"Demo"}]);

			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/list_servers?f=expanded')
				.respond(200, [{"server_id":1,"server_name":"dbserver-master"},{"server_id":2,"server_name":"dbserver-slave"}]);

			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/list_instances?f=expanded&p=%7B%22customer_id%22%3A7%2C%22server_id%22%3A1%7D')
				.respond(200, [{"instance_id":4,"port":5432,"instance_name":"master:5432"}]);

			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/list_databases?f=expanded&p=%7B%22customer_id%22%3A7%2C%22instance_id%22%3A4%2C%22server_id%22%3A1%7D')
				.respond(200, [{"database_id":0,"database_name":"(Todas as bases)"},{"database_id":1,"database_name":"erp"},{"database_id":2,"database_name":"venda"},{"database_id":3,"database_name":"cartaofidel"},{"database_id":4,"database_name":"cotepe"},{"database_id":5,"database_name":"edivenda"},{"database_id":6,"database_name":"sas"},{"database_id":7,"database_name":"sintegra"},{"database_id":8,"database_name":"processos"}]);

			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/list_dates?p=%7B%22customer_id%22%3A7%2C%22server_id%22%3A1%2C%22instance_id%22%3A4%7D')
				.respond(200, {"meta":[{"name":"min","type":"date"},{"name":"?column?","type":"date"},{"name":"max","type":"date"}],"rows":[["2014-08-14T18:27:00.000Z","2014-09-05T14:00:00.000Z","2014-09-05T22:00:00.000Z"]]});

			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/overview_alerts?p=%7B%22customer_id%22%3A7%2C%22instance_id%22%3A4%2C%22server_id%22%3A1%2C%22database_id%22%3A0%2C%22date_from%22%3A%222014-09-05T14%3A00%3A00.000Z%22%2C%22date_to%22%3A%222014-09-05T22%3A00%3A00.000Z%22%7D')
				.respond(200, {"meta":[{"name":"server_name","type":"text"},{"name":"critical","type":"int"},{"name":"warning","type":"int"},{"name":"information","type":"int"}],"rows":[]});

			//var scope = $rootScope.$new();
			var mainCtrl = createMainCtrl();//$controller('MainCtrl', {'$scope': scope});
			expect(scope.getCurrentSectionAtLevel(1)).toBeNull();
			$httpBackend.flush();
			/* Correct section and title? */
			expect(scope.currentSection).toBe('overview');
			expect(document.title).toBe('overview - pgAnalytics');
			expect(scope.selectCharts01).toEqual({key: 'overview_alerts', extraParam: { change_section: { section: 'overview.details', params: [ 'server_name' ] } }, type: 'dataTable' });

			/* Let's move to "pgstats" section */
			$httpBackend
				.expect('GET', '/pga/s/sql/ctm_demo/pgstats_globalcacheratio?p=%7B%22customer_id%22%3A7%2C%22instance_id%22%3A4%2C%22server_id%22%3A1%2C%22database_id%22%3A0%2C%22date_from%22%3A%222014-09-05T14%3A00%3A00.000Z%22%2C%22date_to%22%3A%222014-09-05T22%3A00%3A00.000Z%22%7D')
				.respond(200, {"meta":[{"name":"server_name","type":"text"},{"name":"critical","type":"int"},{"name":"warning","type":"int"},{"name":"information","type":"int"}],"rows":[]});
			scope.setCurrentSection('pgstats');
			$httpBackend.flush();
			expect(scope.currentSection).toBe('pgstats');
			expect(document.title).toBe('pgstats - pgAnalytics');
			expect(scope.selectCharts01).toEqual({key: 'pgstats_globalcacheratio', extraParam: {unit: "mb"}, type: 'stackedAreaChart' });

			/* Test a non-existent section */
			scope.setCurrentSection('foo'); // non-existent section
			expect(scope.currentSection).toBe('pgstats'); // still "pgstats"

			scope.setCurrentSection('pgstats.checkpoints');
			expect(scope.getCurrentSectionAtLevel(0)).toBe('pgstats');
			expect(scope.getCurrentSectionAtLevel(1)).toBe('pgstats.checkpoints');
			expect(scope.getCurrentSectionAtLevel(2)).toBeNull();
			scope.setCurrentSection('sql');
		});
	});
});

