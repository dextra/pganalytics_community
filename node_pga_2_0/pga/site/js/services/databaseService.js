function DatabaseService($http, $q) {

	GenericService.extend(this, $http, $q);

	this.getCustomerSchema = function() {
		return $http.get(this.baseUrl + '/public/list_customer_schema?f=expanded');
	};

	this.getServersBySchema = function(customerSchema) {
		return $http.get(this.baseUrl + '/' + customerSchema + '/list_servers?f=expanded');
	};

	this.getInstances = function(json) {
		var queryParams = {
			'customer_id': json.customer,
			'server_id': json.server
		};

		return $http.get(this.baseUrl + '/' + json.schema + '/list_instances?f=expanded&p=' + encodeURIComponent(JSON.stringify(queryParams)));
	};

	this.getDatabases = function(json) {
		var queryParams = {
			'customer_id': json.customer,
			'instance_id': json.instance,
			'server_id': json.server
		};

		return $http.get(this.baseUrl + '/' + json.schema + '/list_databases?f=expanded&p=' + encodeURIComponent(JSON.stringify(queryParams)));
	};

	this.getCharts = function() {
		return $http.get(this.baseUrl + '/public/list_charts?');
	};

	this.getDetails = function(json) {
		var queryParams = {
			'customer_id': json.customer,
			'instance_id': json.instance,
			'server_id': json.server,
			'database_id' : json.database,
			'date_from': json.from,
			'date_to': json.to
		};

		return $http.get(this.baseUrl + '/' + json.schema + '/' + json.extraParam + '?p=' + encodeURIComponent(JSON.stringify(queryParams)));
	};

	this.getSqlTable = function(json) {
		var queryParams = {
			'instance_id': json.instance,
			'server_id': json.server,
			'database_id' : json.database,
			'date_from': json.from,
			'date_to': json.to
		};		

		return $http.get(this.baseUrl + '/' + json.schema + '/pglog_duration_table?p=' + encodeURIComponent(JSON.stringify(queryParams)));
	};

	this.getWidgetURL = function(json, extraFilters, extraParamName, extraParamValue, format) {
		var queryParams = {
			"customer_id": json.customer,
			"instance_id": json.instance,
			"server_id": json.server,
			"database_id": json.database,
			"date_from": json.from,
			"date_to": json.to
		};

		for (param in extraFilters) {
			queryParams[param] = extraFilters[param];
		}

		if (extraParamName && extraParamValue) {
			queryParams[extraParamName] = extraParamValue;
		}

		format = (format ? '&format=' + encodeURIComponent(format) : '');
		return this.baseUrl + '/' + json.schema + '/' + json.chart + '?p=' + encodeURIComponent(JSON.stringify(queryParams)) + format;
	}

	this.getWidgetData = function(json, extraFilters, extraParamName, extraParamValue, format) {
		return $http.get(this.getWidgetURL(json, extraFilters, extraParamName, extraParamValue, format));
	};

	this.getDates = function(json) {
		var queryParams = {
			"customer_id": json.customer,
			"server_id": json.server,
			"instance_id": json.instance
		};

		return $http.get(this.baseUrl + '/' + json.schema + '/list_dates?p=' + encodeURIComponent(JSON.stringify(queryParams)));
	}

	this.getWidgetHelp = function(lang, key) {
		return $http.get(this.baseUrl + '/public/i18n_gethelp?f=expanded&p=' + encodeURIComponent(JSON.stringify({"language":lang, "key": key})));
	};
}
