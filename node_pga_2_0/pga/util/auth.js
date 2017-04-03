var util = require('./util');
var dbutil = require('./dbutil');
var email = require('./email');
var i18n = require('./i18n');

var TIMEOUT = 86400000; //24 hours

function prepare(req, resp, next) {

	req.userInfo = null;

	function parseToken(token) {
		if (!token) {
			return next();
		}
		dbutil.select({
			resp : resp,
			dbname : 'master',
			sql : 'SELECT id, base FROM userinfo WHERE id = $1',
			params : [ token.u ],
			first : function(userInfo) {
				req.userInfo = userInfo;
				return next();
			}
		});
	}

	function authToken() {
		var token = req.getSignedCookie('pga');
		if (token) {
			token = JSON.parse(token);
			console.log(token);
		}
		if (!token) {
			return next();
		}
		var now = new Date().getTime();
		var expired = token.t + TIMEOUT - now;
		var renew = token.t + (TIMEOUT / 2) - now;
		console.log('timeout', expired, renew);
		if (expired < 0) {
			token = null;
		}
		if (renew < 0 && expired > 0) {
			console.log('token.u: '+ token);
			setToken(token.u);
		} else {
			parseToken(token);
		}
	}

	function setToken(username) {
		var token = {
			u : username,
			t : new Date().getTime()
		};
		var value = resp.setSignedCookie('pga', JSON.stringify(token));
		req.cookies.pga = value;
		parseToken(token);
	}

	function generateResetToken(username) {
		dbutil.select({
			resp: resp,
			dbname: 'master',
			sql: 'WITH usr AS '
				+ '(SELECT * FROM (SELECT u.id, u.email, count(*) OVER() FROM userinfo u WHERE u.email IS NOT NULL AND (u.email = $1 OR u.id = $1)) t WHERE t.count = 1)'
				+ ', ins AS (INSERT INTO password_reset(user_id) SELECT u.id FROM usr u RETURNING user_id, token)'
				+ 'SELECT ins.user_id, ins.token, usr.email FROM usr JOIN ins ON usr.id = ins.user_id',
			params: [ username ],
			first: function(data) {
				var sendMailCallback = function(err) {
					if (err) {
						console.log('sendMail: ', err);
						return util.sendInternalError(resp, 'falha no envio de e-mail');
					}
					resp.redirect('/pga/login.html?infomsg=infosentreset');
				};
				if (data && data.token && data.email && data.user_id) {
					var msg = i18n.resetToken.message(data.user_id, data.token, 'https://sistemas.pganalytics.com.br/');
					email.sendMail({
						to: data.email,
						subject: 'Token para reiniciar senha',
						text: msg.text,
						html: msg.html
					}, sendMailCallback);
				} else {
					/* XXX: Even if the user/email was not found, for security, we still inform the user everything was fine. */
					sendMailCallback(null);
				}
			}
		});
	}

	function validateAndApplyResetToken(username, token, new_password) {
		/* TODO: Use the command bellow in a transaction or database function for better error handling */
		/* TODO: Validate password */
		dbutil.select({
			resp : resp,
			dbname : 'master',
			sql :
				'WITH check_token AS ('
				+ '    UPDATE password_reset'
				+ '    SET reseted_datetime = now()'
				+ '    WHERE user_id = $1 AND token = $2 AND expire_datetime > now() AND reseted_datetime IS NULL'
				+ '    RETURNING user_id'
				+ '), update_password AS ('
				+ '    UPDATE userinfo'
				+ '    SET password = $3'
				+ '    WHERE id IN (SELECT user_id FROM check_token)'
				+ '    RETURNING 1'
				+ ')'
				+ 'SELECT count(*) FROM update_password'
			,
			params : [ username, token, new_password ],
			first : function(data) {
				if (data && data.count == 1) {
					resp.redirect('/pga/login.html?infomsg=inforesetdone');
				} else {
					resp.redirect('/pga/login.html?errmsg=errresetfail');
				}
			}
		});
	}

	function authUserPass() {
		var queryUser = req.query._fuser;
		var body;

		if (queryUser) {
			body = {
				_fuser : req.query._fuser,
				_fpass : req.query._fpass
			};
		} else {
			body = req.body;
		}

		var username = body._fuser || body.username;

		/* TODO: Use the URL at util/server.js */
		if (req.path == '/pga/s/auth/resettoken' && username) {
			/* Generate a reset password token */
			return generateResetToken(username);
		}

		if (req.path == '/pga/s/auth/validateresettoken' && username) {
			var new_pass1 = body.password;
			var new_pass2 = body.confirm_password;
			if (new_pass1 != new_pass2) {
				return util.sendError(resp, 500, 'Password and confirmation mismatch');
			}
			return validateAndApplyResetToken(username, body.token, util.pass(new_pass1));
		}

		if (!username) {
			return authToken();
		}

		var password = body._fpass || body.password;
		password = util.pass(password);
		console.log('expecting: ', password);

		dbutil.select({
			resp : resp,
			dbname : 'master',
			sql : 'SELECT password FROM userinfo WHERE id = $1',
			params : [ username ],
			first : function(loaded) {
				if (!loaded || !loaded.password || loaded.password != password) {
					return util.sendError(resp, 401, 'User and password unauthorized');
				} else {
					setToken(username);
				}
			}
		});
	}

	authUserPass();
}

exports.generateResetToken = function(username, callback) {
	dbutil.select({
		resp : resp,
		dbname : 'master',
		sql : 'INSERT INTO password_reset(user_id) SELECT u.id FROM userinfo u WHERE u.email = $1 OR u.id = $1 RETURNING token',
		params : [ username ],
		first : function(data) {
			if (!data || !data.token) {
				callback(data.token);
			}
		}
	});
};

exports.prepare = function() {
	return prepare;
};

