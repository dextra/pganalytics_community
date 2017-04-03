var t = require('qunitjs');
var http = require('http');
var server = require('../../pga/util/server');
var db = require('../../pga/util/db');

var args = process.argv.slice(2);
var dburl = args[0] || ':pganalytics@localhost:5432/pganalytics';

exports.begin = function() {
	db.setUrl(dburl)
	server.opts.port = 0;
	server.init(function() {
	});
}

exports.done = function(result) {
	t.stop()
	server.close(function() {
		t.start();
	});
}

exports.req = function(type, uri, data, callback) {
	t.stop();
	var options = {
		hostname : 'localhost',
		port : server.opts.port,
		path : uri,
		method : type
	};

	var req = http.request(options, function(resp) {
		var str = ''
		resp.on('data', function(chunk) {
			str += chunk;
		});

		resp.on('end', function() {
			t.start();
			str = str.replace(/^\s+|\s+$/g, '');
			callback(resp, str);
		});
	});

	req.on('error', function(e) {
		t.start();
		t.ok(false, 'error on http: ' + type + ' ' + uri + ': ' + JSON.stringify(e));
	});

	// write data to request body
	// req.write('data\n');
	// req.write('data\n');
	req.end();
}
