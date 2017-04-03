var i18n = {
	errlogin: 'Usuário ou senha inválidos, por favor tente novamente',
	inforesetdone: 'Sua nova senha foi atualizada com sucesso!',
	errresetfail: 'Não foi possível reiniciar a senha com os dados informados. Isso pode significar um erro nos dados ou que seu token já foi expirado (tokens têm validade de 24 horas), por favor verifique os dados informados ou tente uma nova requisição clicando em "esqueceu sua senha?" na tela de login. Em caso de dúvidas ou problemas, entre em contato com dba@pganalytics.com.br.',
	infosentreset: 'Foi enviado uma mensagem para o e-mail cadastrado com informações sobre como gerar uma nova senha!',
	monthNames: ['Jan', 'Fev', 'Mar', 'Abr', 'Mai', 'Jun', 'Jul', 'Ago', 'Set', 'Out', 'Nov', 'Dez'],
	formatter: {
		chartTime: function(dateobj) {
			if (dateobj === null || dateobj === undefined)
				return "NULL";
			return dateobj.getHours().toString().lpad(2, '0')
				+ ":" + dateobj.getMinutes().toString().lpad(2, '0')
				;
		},
		chartDateTime: function(dateobj) {
			if (dateobj === null || dateobj === undefined)
				return "NULL";
			return (dateobj.getDate()).toString().lpad(2, '0')
				+ "/" + i18n.monthNames[dateobj.getMonth()]
				+ " " + dateobj.getHours().toString().lpad(2, '0')
				+ ":" + dateobj.getMinutes().toString().lpad(2, '0')
				;
		},
		fullDateTime: function(dateobj) {
			if (dateobj === null || dateobj === undefined)
				return "NULL";
			return dateobj.getDate().toString().lpad(2, '0')
				+ "/" + (dateobj.getMonth()+1).toString().lpad(2, '0')
				+ "/" + dateobj.getFullYear().toString().lpad(4, '0')
				+ " " + dateobj.getHours().toString().lpad(2, '0')
				+ ":" + dateobj.getMinutes().toString().lpad(2, '0')
				+ ":" + dateobj.getSeconds().toString().lpad(2, '0')
				;
		},
		decimal: function(d, digits) {
			if (d === null || d === undefined)
				return "NULL";
			d = Number(d);
			if (isNaN(d)) return "??";
			var p10 = Math.pow(10, digits);
			d = Math.round(d * p10) / p10;
			if (digits == 0) {
				return Math.round(d) + '';
			} else {
				return parseInt(d) + ',' + parseInt((d * p10) % p10).toString().rpad(digits, '0');
			}
		},
		numericUnit: function(d, unit) {
			if (d === null || d === undefined)
				return "NULL";
			switch (unit) {
				case 'int':
					if (parseInt(d) == d)
						return parseInt(d) + '';
					else
						return '';
				case 's':
					/* Convert to "ms" and go into the next "case" */
					d = parseInt(d);
					if (d < 60) { /* Less than 1 minute */
						return d + ' s';
					} else if (d < (60 * 60)) { /* Less than 1 hour */
						return parseInt(d / 60) + ' min';
					} else {
						return parseInt(d / 60 / 60) + 'h' + (parseInt(d / 60) % 60) + 'min';
					}
				case 'ms':
					d = parseInt(d);
					if (d < 1000) { /* Less than 1 second */
						return d + ' ms';
					} else {
						return i18n.formatter.numericUnit(d / 1000, 's');
					}
				case 'mb':
					if (d < 10000) {
						return i18n.formatter.decimal(d, 1) + ' MB';
					} else if (d < (10 * 1024)) {
						return i18n.formatter.decimal(d, 0) + ' MB';
					} else if (d < (1024 * 1024)) {
						return i18n.formatter.decimal(d/1024.0, 1) + ' GB';
					} else {
						return i18n.formatter.decimal(d/1024.0/1024.0, 1) + ' TB';
					}
				case 'bytes':
					if (parseInt(d) != d)
						return '';
					d = parseInt(d);
					if (d < 8192) {
						return d + ' b';
					} else if (d < (1024 * 1024)) {
						return parseInt(d / 1024) + ' KB';
					} else {
						return i18n.formatter.numericUnit(d / 1024 / 1024, 'mb');
					}
				case 'perc':
					return i18n.formatter.decimal(d, 1) + '%';
				case 'float':
				default:
					return i18n.formatter.decimal(d, 2);
			}
		}
	}
};

var HTTP_STATUS_NAME = {
	'200': 'OK',
	'201': 'Created',
	'202': 'Accepted',
	'203': 'Non-Authoritative Information',
	'204': 'No Content',
	'205': 'Reset Content',
	'206': 'Partial Content',
	'300': 'Multiple Choices',
	'301': 'Moved Permanently',
	'302': 'Found',
	'303': 'See Other',
	'304': 'Not Modified',
	'305': 'Use Proxy',
	'307': 'Temporary Redirect',
	'400': 'Bad Request',
	'401': 'Unauthorized',
	'402': 'Payment Required',
	'403': 'Forbidden',
	'404': 'Not Found',
	'405': 'Method Not Allowed',
	'406': 'Not Acceptable',
	'407': 'Proxy Authentication Required',
	'408': 'Request Timeout',
	'409': 'Conflict',
	'410': 'Gone',
	'411': 'Length Required',
	'412': 'Precondition Failed',
	'413': 'Request Entity Too Large',
	'414': 'Request-URI Too Long',
	'415': 'Unsupported Media Type',
	'416': 'Requested Range Not Satisfiable',
	'417': 'Expectation Failed',
	'500': 'Internal Server Error',
	'501': 'Not Implemented',
	'502': 'Bad Gateway',
	'503': 'Service Unavailable',
	'504': 'Gateway Timeout',
	'505': 'HTTP Version Not Supported'
};

if (!String.prototype.repeat) {
	String.prototype.repeat = function(n) {
		var ret = "";
		for (var i = 0; i < n; i++) {
			ret += this;
		}
		return ret;
	}
}

String.prototype.lpad = function(length, chr) {
	var diff = length - this.length;
	if (diff <= 0) {
		return this.substr(0, length);
	}
	if (!chr) chr = ' ';
	return chr.repeat(Math.ceil(diff / chr.length)).substr(0, diff) + this;
};

String.prototype.rpad = function(length, chr) {
	var diff = length - this.length;
	if (diff <= 0) {
		return this.substr(0, length);
	}
	if (!chr) chr = ' ';
	return this + chr.repeat(Math.ceil(diff / chr.length)).substr(0, diff);
}

function PQQuoteLiteral(value) {
	if (value.match(/^[a-z_]+[a-z_0-9]*$/)) {
		return value;
	} else {
		return '"' + value.replace(/"/g, '""') + '"';
	}
}

function parseURLQuery(query_url) {
	var query = query_url || window.location.search || '';
	var ret = {};
	if (query.match(/^\?/)) {
		var params = query.substr(1).split('&');
		for (var i in params) {
			var p = params[i].split('=');
			if (p.length == 2) {
				ret[decodeURIComponent(p[0])] = decodeURIComponent(p[1].replace(/\+/g, ' '));
			}
		}
	}
	return ret;
}

/* Functions to work with Cookies (Source: http://www.quirksmode.org/js/cookies.html) */

var Cookies = {}
Cookies.create = function(name,value,days) {
	if (days) {
		var date = new Date();
		date.setTime(date.getTime()+(days*24*60*60*1000));
		var expires = "; expires="+date.toGMTString();
	}
	else var expires = "";
	document.cookie = name+"="+value+expires+"; path=/";
};

Cookies.read = function(name) {
	var nameEQ = name + "=";
	var ca = document.cookie.split(';');
	for(var i=0;i < ca.length;i++) {
		var c = ca[i];
		while (c.charAt(0)==' ') c = c.substring(1,c.length);
		if (c.indexOf(nameEQ) == 0) return c.substring(nameEQ.length,c.length);
	}
	return null;
};

Cookies.erase = function(name) {
	Cookies.create(name,"",-1);
};

/* User login verification */

function isUserLogged() {
	try {
		var pga = Cookies.read('pga');
		var now = new Date();
		if (!pga) return false;
		/* TODO: Use a cross-browser version of "atob" function */
		if (!atob) return true;
		pga = JSON.parse(atob(pga.split('.')[0]));
		return (pga.t + 24*60*60*1000) > now.getTime();
	} catch(e) {
		/* XXX: If there is an error we should return "false", but the above *shouldn't* return an error, so the server will check if logged on any request */
		console.error(e);
		return true;
	}
}

function getUserName()
{
	try {
		var pga = Cookies.read('pga');
		pga = JSON.parse(atob(pga.split('.')[0]));
		return pga.u;
	} catch(e)
	{
		//console.error(e);
		return 'NO_USER';
	}
}

function forceUserLogged() {
	if (!isUserLogged()) {
		window.location.href = "/pga/login.html";
	}
}

var GA = {
	create: function() {
		switch(getUserName()) {
			case "admin":
				ga('create', 'UA-44518307-4', 'auto'); /* para a conta do site "secure.pganalytics.com.br" */
				break;
			case "demo":
				ga('create', 'UA-44518307-2', 'auto'); /* para a conta do site "secure.pganalytics.com.br" */
				break;
			default:
				ga('create', 'UA-44518307-3', 'auto'); /* para a conta do site "secure.pganalytics.com.br" */
		};
	},
	createOnce: function() {
		if (!GA._created) {
			GA.create();
			GA._created = true;
		}
	},
	sendPageView: function() {
		GA.createOnce();
		ga('send', 'pageview');
	},
	sendEvent: function(category, action, label) {
		GA.createOnce();
		ga('send', 'event', category, action, label);
	}
};

function findItemByKeyValue(object_array, key, value, default_if_not_found) {
	default_if_not_found = (typeof default_if_not_found !== 'undefined' ? default_if_not_found : -1);
	for (var i = 0; i < object_array.length; i++) {
		var obj = object_array[i];
		if (obj && obj[key] == value) {
			return i;
		}
	}
	return default_if_not_found;
}

function countObjectKeys(obj) {
	if (Object && Object.keys && !window.testIgnoreObjectKeys) {
		return Object.keys(obj).length;
	} else {
		var ret = 0;
		for (var i in obj) {
			if (obj.hasOwnProperty(i)) ret++;
		}
		return ret;
	}
}


