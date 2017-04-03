var pg = require('pg');

var url = ':@/pganalytics?application_name=pganalytics';

function resolveURL(name) {
	var dbuser = '';
	if (name == 'master' || name == 'public') {
		dbuser = 'pga_app_' + name;
	} else {
		dbuser = name + '_ro';
		dbuser = 'ctm_demo_ro';
	}
	return 'postgres://' + dbuser + url;
}

function conn(name, f) {
	var u = resolveURL(name);
	pg.connect(u, f);
}

function setUrl(u) {
	url = u;
}

exports.conn = conn;
exports.setUrl = setUrl;
