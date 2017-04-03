var nodemailer = require('nodemailer');

var transport = null;
var user = 'user';
var pass = 'password';

function setupTransport() {
	if (!transport) {
		transport = nodemailer.createTransport({
			service: 'gmail',
			auth: {
				user: user,
				pass: pass
			}
		});
	}
}

exports.sendMail = function(data, callback) {
	setupTransport();
	if (!data.from) {
		data.from = "pgAnalytics <" + user + ">";
	}
	transport.sendMail(data, callback);
}

