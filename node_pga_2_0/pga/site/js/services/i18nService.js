function i18nService($http, $q) {

	this.data = {};

	GenericService.extend(this, $http, $q);

	this.loadLanguage = function(lang, callback_success, callback_error) {
		$http
			.get(this.baseUrl + '/public/i18n_getall?f=expanded&p=' + encodeURIComponent(JSON.stringify({"language":lang})))
			.then(function(response) {
					if (response.data && response.data.length === 1 && response.data[0].data) {
						this.data = response.data[0].data;
						/* TODO: remove references to old i18n global object (this loops reconstruct it for compatibility) */
						for (var k in this.data) {
							i18n[k] = this.data[k].title;
						}
						callback_success(this.data);
					} else if(callback_error) {
						callback_error("Invalid output");
					}
				}, callback_error
				);
	};

	/*
	this.get = function(key, return_key_if_not_found) {
		if (this.data[key]) return this.data[key];
		return_key_if_not_found = (return_key_if_not_found === undefined ? true : return_key_if_not_found);
		return {
			title: (return_key_if_not_found ? key : null),
			tooltip: null,
			description: null,
			has_help: false
		};
	};

	this.getTitle = function(key, return_key_if_not_found) {
		return this.get(key, return_key_if_not_found).title;
	};

	this.getTooltip = function(key) {
		return this.get(key).tooltip;
	};

	this.getDescription = function(key) {
		return this.get(key).description;
	};

	this.hasHelp = function(key) {
		return this.get(key).has_help;
	};
	*/

}

