exports.resetToken = {
	subject: '[pgAnalytics] Gerar nova senha',
	message: function(userName, token, baseURL) {
		var url = baseURL + "pga/resetpassword.html?token=" + token + "&username=" + userName;
		var msg =
			"Olá,\n"
			+ "Foi realizada uma requisição para gerar uma nova senha para o usuário \"" + userName + "\".\n"
			+ "Para prosseguir, acesse a seguinte URL:\n"
			+ "<a href=\"" + url + "\">" + url + "</a>.\n"
			+ "Caso você não tenha feito esta requisição, por favor ignore esta mensagem, sua senha anterior irá continuar funcionando.\n"
			+ "Em caso de dúvidas, entre em contato.";
		return {
			html: msg.replace(/\n/gm, "<br>"),
			text: msg.replace(/<[^>]*>/g, '')
		};
	}
};

