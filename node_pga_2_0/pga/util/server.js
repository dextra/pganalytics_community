var express = require('express');
var cookieParser = require('cookie-parser');
var path = require('path');
var favicon = require('serve-favicon');
var logger = require('morgan');
var cookieParser = require('cookie-parser');
var bodyParser = require('body-parser');
var cookies = require('./cookies');
var auth = require('./auth');
var auth_service = require('../service/authService')
var autoless = require('./autoless');
var compress = require('compression');


function service(req, resp) {
	var array = req.baseUrl.split('/');
	var serviceName = array[3];
	var service = require('../service/' + serviceName + 'Service');
	return service.execute(req, resp);
}

var opts = {env: "devel"};
const envs = {
	DEVEL: "devel",
	PROD: "prod"
};

opts.filter = function(request, response, next) {
	if(request.userInfo == null) {
		response.status(401);
		auth_service.logout(request, response);
	} else {
		response.set({'userPrincipal':request.userInfo.id});
		next();
	}
}

exports.opts = opts;
exports.envs = envs;

exports.close = function(callback) {
	opts.server.close(function() {
		if(callback) {
			callback();
		}
	});
}

exports.init = function(callback) {
	var app = express();
	app.use(compress());
	app.use(favicon(__dirname + '/../site/favicon.ico'));
	app.use(logger('dev'));
	app.use(cookieParser());
	app.use(cookies.prepare());
	app.use(bodyParser.json());
	app.use(bodyParser.urlencoded());
	app.use(auth.prepare());
	app.use('/pga/index.html', function(req, resp) {
		resp.redirect('/pga/');
	});
	if (opts.env === envs.DEVEL) {
		app.use('/pga/css/pganalytics.css',
			autoless.prepare(
				path.join(__dirname, '../site/css/basic.less'),
				path.join(__dirname, '../site/css/pganalytics.css'),
				path.join(__dirname, '../site/css/')
				)
			);
		app.use('/pga/dashboard/css/dashboard.css',
			autoless.prepare(
				path.join(__dirname, '../test/dashboard/css/dashboard.less'),
				path.join(__dirname, '../test/dashboard/css/dashboard.css'),
				path.join(__dirname, '../test/dashboard/css/')
				)
			);
	}
	app.use('/pga', express.static(path.join(__dirname, '../site')));
	app.use('/pga', express.static(path.join(__dirname, '../test')));
	app.use(function(req, resp, next) {
		return opts.filter(req, resp, next);
	});
	app.use('/pga/s/*', service);
	opts.server = app.listen(opts.port, function() {
		exports.opts.port = opts.server.address().port;
		console.log('Listening on port %d', opts.server.address().port);
		if (callback) {
			callback();
		}
	});
}

