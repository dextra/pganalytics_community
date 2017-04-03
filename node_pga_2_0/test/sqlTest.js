var t = require('qunitjs');
var u = require('./util/testutil');

exports.load = function() {
	// http://localhost:8081/pga/s/sql/ctm_demo/sostats_cpuusage?_fuser=admin&_fpass=123&p={"customer_id":7,"server_id":1,"date_from":"2014-08-01 00:00:00","date_to":"2014-08-30 23:59:59"}
	t.test('check meta', function(a) {
		u.req( 'GET', '/pga/s/sql/ctm_demo/sostats_cpuusage?_fuser=admin&_fpass=123&p=' + encodeURIComponent('{"customer_id":7,"server_id":1,"date_from":"2014-08-01 00:00:00","date_to":"2014-08-30 23:59:59"}')
			  ,null, function(resp, data) {
			            a.equal(resp.statusCode, 200);
		});
	});

	// http://localhost:8081/pga/s/sql/ctm_demo/pgstats_checkpoints?_fuser=admin&_fpass=123&p={"customer_id":7,"server_id":1,"instance_id":4,"database_id":0,"date_from":"2014-08-01 00:00:00","date_to":"2014-08-30 23:59:59"}
	t.test('check meta', function(a){
		u.req('GET', '/pga/s/sql/ctm_demo/pgstats_checkpoints?_fuser=admin&_fpass=123&p=' + encodeURIComponent('{"customer_id":7,"server_id":1,"instance_id":4,"database_id":0,"date_from":"2014-08-01 00:00:00","date_to":"2014-08-30 23:59:59"}')
			  ,null, function(resp, data) {
			            a.equal(resp.statusCode, 200);
		});
	});
	
}
