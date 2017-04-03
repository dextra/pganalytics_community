var crypto = require('crypto');
var fs = require('fs');

exports.sendInternalError = function(resp, err, callback) {
	console.error('sendError', err);
	if (callback) {
		callback();
	}
	resp.writeHead(500, {
		'content-type' : 'text/plain'
	});
	resp.end('Error: ' + err + '\n');
};

exports.sendError = function(resp, code, msg, callback) {
	if (callback) {
		callback();
	}
	
	if(code === 401) {
		resp.redirect('/pga/login.html?errmsg=errlogin');
		/*
		fs.readFile(__dirname + '/../site/login.html', function (erro, html) {
			resp.writeHead({'content-type' : 'text/html'});
			resp.write(html);
			resp.end();
		});
		*/
	} else {
		resp.writeHead(code, {
			'content-type' : 'text/plain'
		});
		resp.end('Error: ' + code + ' ' + msg + '\n');
	}
};

exports.requireMethod = function(req, resp) {
	for (var i = 2; i < arguments.length; i++) {
		if (req.method == arguments[i]) {
			return;
		}
	}
	exports.sendError(resp, 405, 'method not allowed: ' + req.method);
}

exports.splitGet = function(str, c, idx) {
	var array = str.split(c);
	return array[idx];
}

exports.regexGroups = function(regex, str) {
	var ret = [];
	var m = regex.exec(str);
	while (m) {
		ret.push(m[1]);
		m = regex.exec(str);
	}
	return ret;
}

exports.pass = function(password) {
	var hasher = crypto.createHash('sha256');
	if (!password) {
		password = '';
	}
	return hasher.update(password).digest('hex');
}

exports.base64 = {
	encode : function(data) {
		var ret = new Buffer(data, 'binary').toString('base64');
		return ret.replace(/\+/g, '-').replace(/\//g, '_').replace(/=+$/, '');
	},
	decode : function(data, format) {
		data = data.replace(/-/g, '+').replace(/_/g, '/');
		while (data.length % 4) {
			data += '=';
		}
		var ret = new Buffer(data, 'base64');
		if (format) {
			return ret.toString(format);
		}
		return ret;
	}
};

exports.sendJson = function(resp, obj) {
	var json = JSON.stringify(obj);
	var buffer = new Buffer(json, 'UTF-8');
	resp.setHeader('Content-Type', 'application/json; charset=UTF-8');
	resp.setHeader('Content-Length', buffer.length);
	resp.end(buffer);
}
