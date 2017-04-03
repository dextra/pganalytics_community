exports.execute = function(req, resp) {
	var ret = {};
	ret.token = req.cookies[req.query.name];
	ret.value = req.getSignedCookie(req.query.name);
	resp.end(JSON.stringify(ret));
}