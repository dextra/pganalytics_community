var t = require('qunitjs');
var u = require('./util/testutil');

exports.load = function() {

	function execute(a) {
		u.req('GET', '/pga/ping.txt', null, function(resp, data) {
			a.equal(resp.statusCode, 200);
			a.equal(data, 'ping');
			a.equal(resp.headers['content-type'], 'text/plain; charset=UTF-8');
		});
	}
	
	t.test('ping.txt first', execute);
	
	t.test('ping.txt second', execute);

}
