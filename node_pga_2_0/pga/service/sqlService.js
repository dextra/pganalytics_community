var util = require('../util/util');
var dbutil = require('../util/dbutil');
var db = require('../util/db');
var fs = require('fs');

exports.execute = function(req, resp) {

	var opts = {};
	var array = req.baseUrl.split('/');
	opts.dbName = array[4];
	opts.queryName = array[5];
	opts.params = JSON.parse(req.query.p || '{}');

	if (!opts.dbName) {
		return util.sendError(resp, 500, 'wrong');
	}
	if (!req.userInfo) {
		return util.sendError(resp, 401, 'auth is required');
	}
	if (!opts.dbName.match(/^(ctm_.*|public)$/)) {
		return util.sendError(resp, 500, 'Invalid dbName: ' + opts.dbName);
	}
	if (!opts.dbName.match(req.userInfo.base) && opts.dbName != 'public') {
		return util.sendError(resp, 403, 'You cannot access: ' + opts.dbName);
	}

	opts.params["user_info.name"] = req.userInfo.id;
	opts.params["user_info.base"] = req.userInfo.base;

	function sqlRead(err, sqlString) {
		if (err) {
			if (err.errno == 34) {
				return util.sendError(resp, 404, 'sql not found: ' + opts.queryName);
			}
			return util.sendInternalError(resp, err);
		}

		var datatable_request = false;

		if (req.query.f && !req.query.format) {
			req.query.format = req.query.f;
		}

		/* TODO: Refactoring the way of handling different formats, specially 'dataTableServerSide' */
		if (req.query.format && req.query.format == 'dataTableServerSide') {
			if (!req.query.draw) {
				/* First request is used only to retrieve the query meta data */
				sqlString = 'SELECT * FROM (' + sqlString + ') t LIMIT 0';
			} else {
				req.query.draw = parseInt(req.query.draw);
				datatable_request = true;
			}
		}

		db.conn(opts.dbName, function(err, client, done) {
			if (err) {
				return util.sendInternalError(resp, err, done);
			}

			var st = dbutil.namedParams(sqlString, opts.params);
			if (!datatable_request) {
				var query = client.query(st.sql, st.params);
				if (req.query.format && req.query.format == 'expanded') {
					dbutil.sendExpandedJson(resp, query, done);
				} else {
					dbutil.sendJson(resp, query, done);
				}
			} else {
				var total_rows;
				/* TODO: See if this "count" can be avoided, either by caching or some other way */
				client
					.query('SELECT count(*) FROM (' + st.sql + ') t', st.params)
					.on('error', function(err) {
						return util.sendInternalError(resp, err, done);
					})
					.on('row', function(row, result) {
						total_rows = row.count;
					})
					.on('end', function(result) {
						var sql = 'SELECT * FROM (\n' + st.sql + '\n) t';
						/* TODO: Search/filtering */
						if (req.query.search && req.query.search.value) {
							var search_param_number = st.params.length + 1;
							var first = true;
							console.log(req.query.search.value);
							st.params.push('%' + req.query.search.value + '%');
							for (var i in req.query.columns) {
								if (req.query.columns[i].name && req.query.columns[i].searchable && req.query.columns[i].searchable === 'true') {
									if (first) {
										sql += '\nWHERE\n(\n';
										first = false;
									} else {
										sql += '\n\tOR ';
									}
									sql += 'CAST(' + dbutil.quoteLiteral(req.query.columns[i].name) + ' AS text) LIKE $' + search_param_number;
								}
							}
							if (!first) {
								sql += '\n)';
							}
						}
						if (req.query.order && req.query.order[0] && !isNaN(parseInt(req.query.order[0].column))) {
							sql += '\nORDER BY ' + (parseInt(req.query.order[0].column) + 1);
							if (req.query.order[0].dir && req.query.order[0].dir == "desc") {
								sql += ' DESC NULLS LAST';
							}
						}
						if (req.query.length && !isNaN(parseInt(req.query.length))) {
							sql += '\nLIMIT ' + parseInt(req.query.length);
						}
						if (req.query.start && !isNaN(parseInt(req.query.start))) {
							sql += '\nOFFSET ' + parseInt(req.query.start);
						}
						var query = client.query(sql, st.params);
						dbutil.sendDatatableJson(resp, req.query.draw, total_rows, query, done);
					});
			}
		});
	}

	fs.readFile('pga/sql/' + opts.queryName + '.sql', 'utf-8', sqlRead);
}

