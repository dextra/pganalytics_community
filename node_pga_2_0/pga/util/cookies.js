var crypto = require('crypto');
var kp = require('keypair');
var dbutil = require('./dbutil');
var util = require('./util');

var keypair = null;

function setSignedCookie(name, value) {
	var signer = crypto.createSign('RSA-SHA256');
	signer.update(value);
	var sign = signer.sign(keypair.privatekey, 'binary');
	var sign = util.base64.encode(sign);
	value = util.base64.encode(value);
	var token = value + '.' + sign;
	this.cookie(name, token);
	return token;
}

function getSignedCookie(name) {
	var token = this.cookies[name];
	if (!token) {
		return token;
	}
	var array = token.split('.');
	var value = array[0];
	var sign = array[1];
	value = util.base64.decode(value, 'UTF-8');
	sign = util.base64.decode(sign, 'binary');
	var verifier = crypto.createVerify('RSA-SHA256');
	verifier.update(value);
	var ok = verifier.verify(keypair.publickey, sign, 'binary');
	return ok ? value : null;
}

function prepare(req, resp, next) {
	req.getSignedCookie = getSignedCookie;
	resp.setSignedCookie = setSignedCookie;

	if (keypair != null) {
		return next();
	}

	function keyloaded(loaded) {
		if (loaded) {
			keypair = loaded;
			return next();
		}
		var _keypair = kp({
			bits : 512
		});
		dbutil.select({
			resp : resp,
			dbname : 'master',
			sql : "INSERT INTO keypair (id, privatekey, publickey) VALUES ('master', $1, $2)",
			params : [ _keypair['private'], _keypair['public'] ],
			count : function(count) {
				keypair = {
					id : 'master',
					privatekey : _keypair['private'],
					publickey : _keypair['public']
				};
				next();
			}
		});
	}

	dbutil.select({
		resp : resp,
		dbname : 'master',
		sql : "SELECT id, publickey, privatekey FROM keypair WHERE id = 'master'",
		first : keyloaded
	});

}

exports.prepare = function() {
	return prepare;
}
