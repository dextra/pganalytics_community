var util = require('../util/util');
var dbutil = require('../util/dbutil');
var cookies = require('../util/cookies');

exports.logout = function (req, resp) {
	resp.clearCookie('pga');
	resp.end();
}

exports.execute = function(req, resp) {

	function doLogin() {
		var ret = {
			userInfo: req.userInfo,
			token: null
		};
		if (req.userInfo) {
			ret.token = req.cookies['pga'];
		}
		resp.redirect('/pga/');
	}

	function doLogout() {
		resp.clearCookie('pga');
		resp.redirect('/pga/login.html');
	}

	var cmd = util.splitGet(req.baseUrl, '/', 4);
	switch (cmd) {
		case 'login':
			return doLogin();
		case 'logout':
			return doLogout();
	}

	return util.sendError(resp, 404, 'cmd not found: ' + cmd);
}

