var util = require('./util');
var fs = require('fs');
var child_process = require('child_process');
var less = require('less');

function AutoLess(lessinput, cssoutput, dependenciesDir) {

	this.prepare = function(req, resp, next) {
		var rebuild = !fs.existsSync(cssoutput);
		if (!rebuild) {
			var css_stat = fs.statSync(cssoutput);
			var files = fs.readdirSync(dependenciesDir);
			for (var i in files) {
				if (files[i].match(/\.less$/)) {
					if (fs.statSync(dependenciesDir + '/' + files[i]).mtime.getTime() > css_stat.mtime.getTime()) {
						rebuild = true;
						break;
					}
				}
			}
		}
		if (rebuild) {
			console.log(cssoutput, ' needs rebuild');
			fs.readFile(lessinput, "utf8", function(err, data) {
				if (err) {
					util.sendError(resp, 500, err)
					return;
				}
				less.render(data,
					{
						paths: [ dependenciesDir ],
						filename: lessinput,
						dumpLineNumbers: "comments",
						compress: false
					}, function(err, ret) {
						if (err) {
							util.sendError(resp, 500, err)
							return;
						}
						fs.writeFile(cssoutput, ret.css, function(err){
							if (err) {
								util.sendError(resp, 500, err)
								return;
							}
							next();
						});
					}
				);
			});
		} else {
			next();
		}
	}
	return this;
}

exports.prepare = function(lessinput, cssoutput, dependenciesDir) {
	var al = new AutoLess(lessinput, cssoutput, dependenciesDir);
	return al.prepare;
};


