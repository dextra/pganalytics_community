var util = require('./util');
var db = require('./db');

function resolveType(field) {
	var id = field.dataTypeID;
	//console.log('Tipo de dado::' + id + '<<');
	switch (id) {
		case 1043: // varchar
		case 19: // name
		case 25: // text
			return 'text';
		case 20: // bigint
		case 23: // int
			return "int";
		case 701:   // double precision
			return "float";
		case 1700: // numeric
			return "numeric";
		case 1184: // timestamp with time zone
			return "date";
			//return "timestamp with time zone"
		case 16:
			return "boolean";
	}
	throw 'unknown datatypeid: ' + id;
}

function resolveMeta(r) {
	var ret = [];
	for (var i = 0; i < r.fields.length; i++) {
		var field = r.fields[i];
		var col = {
			name : field.name,
			type : resolveType(field)
		};
		ret.push(col);
	}
	return ret;
}

function sendJson(resp, query, done) {
	var meta = [];

	function prepare(result) {
		meta = resolveMeta(result);
		ret += '{"meta":';
		ret += JSON.stringify(meta);
		ret += ',"rows":[';
	}

	var rownum = 0;
	var ret = '';
	query.on('error', function(err) {
		return util.sendInternalError(resp, err, done);
	});
	query.on('row', function(row, result) {
		if (rownum == 0) {
			prepare(result);
		}
		if (rownum >= 1) {
			ret += ',';
		}
		var line = [];
		for (var i = 0; i < result.fields.length; i++) {
			var field = result.fields[i].name;
			var value = row[field];
			if (value && meta[i].type == "date" && value.toISOString) {
				value = value.toISOString();
			}
			line.push(value);
		}
		ret += JSON.stringify(line)
		rownum++;
	});
	query.on('end', function(result) {
		if (rownum == 0) {
			prepare(result);
		}
		ret += ']}';
		var buffer = new Buffer(ret, 'UTF-8');
		resp.setHeader('Content-Type', 'application/json; charset=UTF-8');
		resp.setHeader('Content-Length', buffer.length);
		resp.end(buffer);
		done();
	});
}

function namedParams(sql, params) {
	var regex = /\$\{([^}]+)}/g;
	var ret = {};
	ret.sql = sql;
	ret.params = [];
	var names = util.regexGroups(regex, sql);
	for (var i = 0; i < names.length; i++) {
		var name = names[i];
		var value = params[name];
		ret.sql = ret.sql.replace('${' + name + '}', '$' + (i + 1));
		ret.params.push(value);
	}
	return ret;
}

function select(opts) {
	db.conn(opts.dbname, function(err, client, done) {

		function sendError(err) {
			console.log('x', err);
			if (opts.resp) {
				return util.sendInternalError(opts.resp, err, done);
			}
			if (opts.error) {
				return opts.error(err, client, done);
			}
			done();
			throw err;
		}

		if (err) {
			return sendError(err);
		}

		function loaded(err, result) {
			if (err) {
				return sendError(err);
			}
			if(opts.count) {
				opts.count(result.rowCount);
			}
			var first = result.rows.length ? result.rows[0] : null;
			if (opts.reqFirst && !first) {
				sendError({
					msg : 'one row is required'
				});
			}
			if (opts.first) {
				opts.first(first);
			}
			if (opts.reqFirst) {
				opts.reqFirst(first);
			}
			done();
		}

		var query = client.query(opts.sql, opts.params, loaded);
	});
}

function sendDatatableJson(resp, draw, total_rows, query, done) {
	var rownum = 0;
	var ret = '{"draw": ' + draw + ', "recordsTotal": ' + total_rows + ', "recordsFiltered": ' + total_rows + ', "data": [';
	query.on('error', function(err) {
		return util.sendInternalError(resp, err, done);
	});
	query.on('row', function(row, result) {
		if (rownum) ret += ',';
		ret += '[';
		for (var i = 0; i < result.fields.length; i++) {
			if (i) ret += ',';
			ret += JSON.stringify(row[result.fields[i].name]);
		}
		ret += ']';
		rownum++;
	});
	query.on('end', function(result) {
		ret += ']}';
		var buffer = new Buffer(ret, 'UTF-8');
		resp.setHeader('Content-Type', 'application/json; charset=UTF-8');
		resp.setHeader('Content-Length', buffer.length);
		resp.end(buffer);
		done();
	});
}

function sendExpandedJson(resp, query, done) {
	var rownum = 0;
	var ret = '[';
	query.on('error', function(err) {
		return util.sendInternalError(resp, err, done);
	});
	query.on('row', function(row, result) {
		if (rownum) ret += ',';
		ret += '{';
		for (var i = 0; i < result.fields.length; i++) {
			if (i) ret += ',';
			ret += JSON.stringify(result.fields[i].name) + ':' + JSON.stringify(row[result.fields[i].name]);
		}
		ret += '}';
		rownum++;
	});
	query.on('end', function(result) {
		ret += ']';
		var buffer = new Buffer(ret, 'UTF-8');
		resp.setHeader('Content-Type', 'application/json; charset=UTF-8');
		resp.setHeader('Content-Length', buffer.length);
		resp.end(buffer);
		done();
	});
}

function quoteLiteral(value) {
	if (value.match(/^[a-z_]+[a-z_0-9]*$/)) {
		return value;
	} else {
		return '"' + value.replace(/"/g, '""') + '"';
	}
}

exports.sendJson = sendJson;
exports.sendDatatableJson = sendDatatableJson;
exports.sendExpandedJson = sendExpandedJson;
exports.namedParams = namedParams;
exports.select = select;
exports.quoteLiteral = quoteLiteral;

