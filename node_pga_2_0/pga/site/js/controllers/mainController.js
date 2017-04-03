(function(angular) {
'use strict';

var pganalytics = angular.module('pganalytics');
pganalytics.controller('MainCtrl', ['$scope', '$sce', '$interpolate', 'databaseService', 'i18nService', function($scope, $sce, $interpolate, databaseService, i18nService) {

	$scope.initMain = function() {
		$scope.logo = 'dextra';
		$scope.mainNavigation = {
			currStep: -1,
			STARTING: -1,
			LOADING_WIDGETS: 0,
			LOADING_CUSTOMER: 1,
			LOADING_SERVER: 2,
			LOADING_INSTANCE: 3,
			LOADING_DATABASE: 4,
			LOADING_DATETIME: 5,
			LOADING_MAIN_WIDGET: 6,
			LOADED_ALL: 7,
			stepMeta: [
				{
					name: "charts",
					loadCallback: $scope.listCharts
				},
				{
					name: "customer",
					loadCallback: $scope.listCustomerSchemas,
					currentVar: "selectCustomer01"
				},
				{
					name: "server",
					loadCallback: $scope.listServers,
					currentVar: "selectServer01"
				},
				{
					name: "instance",
					loadCallback: $scope.changeInstances,
					currentVar: "selectInstance01",
				},
				{
					name: "database",
					loadCallback: $scope.changeDatabases,
					currentVar: "selectDatabase01"
				},
				{
					name: "dates",
					loadCallback: $scope.filterDates
				},
				{
					name: "mainWidget",
					loadCallback: $scope.loadMainWidget
				},
				{
					name: "end",
					loadCallback: $scope.saveURL
				}
			]
		};

		$scope.showDatepicker = true;

		$scope.customers = null;
		$scope.servers = null;
		$scope.instances = null;
		$scope.databases = null;
		$scope.dateFrom = null;
		$scope.dateTo = null;
		$scope.sections = {};
		$scope.currentSection = null;
		$scope.charts = null;
		$scope.chartsDetails = [];
		$scope.extraParamName = null;
		$scope.extraFilters = {};
		$scope.currentURL = null;
		$scope.widgetApi = {};
		$scope.defaultFilterValues = parseURLQuery(window.location.hash.substr(1)) || {};
		if ($scope.defaultFilterValues.filters) {
			/* TODO: Validate the URL */
			$scope.extraFilters = JSON.parse($scope.defaultFilterValues.filters);
		}

		installMainNavigationWatches();
		$scope.listCharts();
		//$scope.listCustomerSchemas();

		/* Datepicker definitions */
		$scope.format = 'dd/MM/yyyy';
		$scope.dateOptions = {
			formatYear: 'yy',
			startingDay: 1
		};

		$scope.hstep = 1;
		$scope.mstep = 10;

		/* Inherited controllers can set this to ignore some keys in the final URL */
		$scope.ignoreKeysForURL = {schema: true};
	}

	$scope.listCustomerSchemas = function() {
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_CUSTOMER;
		databaseService
			.getCustomerSchema()
			.success(function(data, status, headers, config) {
				$scope.customers = data;

				if ($scope.customers.length > 0) {
					if ($scope.defaultFilterValues.customer) {
						$scope.selectCustomer01 = $scope.customers[findItemByKeyValue($scope.customers, "customer_id", $scope.defaultFilterValues.customer, 0)];
					} else {
						$scope.selectCustomer01 = $scope.customers[0];
					}
				}
				$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_CUSTOMER + 1;
			})
			.error(loadingHttpError);
	}

	$scope.listServers = function() {
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_SERVER;
		if ($scope.selectCustomer01 != undefined) {
			$scope.logo = $scope.selectCustomer01.name_id;
			var customerSchema = $scope.selectCustomer01.schema;

			databaseService
				.getServersBySchema(customerSchema)
				.success(function(data, status, headers, config) {
					$scope.servers = data;
					if ($scope.defaultFilterValues.server) {
						$scope.selectServer01 = $scope.servers[findItemByKeyValue($scope.servers, "server_id", $scope.defaultFilterValues.server, 0)];
					} else {
						$scope.selectServer01 = $scope.servers[0];
					}
					$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_SERVER + 1;
					//$scope.changeInstances();
				})
				.error(loadingHttpError);
		}
	}

	$scope.changeInstances = function() {
		var dbInfo = $scope.getDatabaseInfo();
		if (dbInfo.customer && dbInfo.server) {
			$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_INSTANCE;
			databaseService
				.getInstances(dbInfo)
				.success(function(data, status, headers, config) {
					$scope.instances = data;
					if ($scope.defaultFilterValues.instance) {
						$scope.selectInstance01 = $scope.instances[findItemByKeyValue($scope.instances, "instance_id", $scope.defaultFilterValues.instance, 0)];
					} else {
						$scope.selectInstance01 = $scope.instances[0];
					}
					$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_INSTANCE + 1;
					//$scope.changeDatabases();
				})
				.error(loadingHttpError);
		}
	}

	$scope.changeDatabases = function() {
		var dbInfo = $scope.getDatabaseInfo();
		if (dbInfo.customer && dbInfo.instance && dbInfo.server) {
			$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_DATABASE;
			databaseService
				.getDatabases(dbInfo)
				.success(function(data, status, headers, config) {
					$scope.databases = data;
					if ($scope.defaultFilterValues.database) {
						$scope.selectDatabase01 = $scope.databases[findItemByKeyValue($scope.databases, "database_id", $scope.defaultFilterValues.database, 0)];
					} else {
						$scope.selectDatabase01 = $scope.databases[0];
					}
					$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_DATABASE + 1;
				})
				.error(loadingHttpError);
		}
	}

	$scope.listCharts = function() {
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_WIDGETS;
		i18nService.loadLanguage("pt_BR", function(i18n_data) {

			$scope.i18n = i18n_data;

			databaseService
				.getCharts()
				.success(function(data, status, headers, config) {
					var charts = [];
					for (var c in data.rows) {
						var section_name = data.rows[c][1];
						if (!$scope.sections[section_name]) {
							$scope.sections[section_name] = {
								key: section_name,
								charts: []
							};
						}
						$scope.sections[section_name].charts.push({
							key: data.rows[c][0],
							extraParam: (!data.rows[c][2] ? {} : JSON.parse(data.rows[c][2])),
							type: data.rows[c][3]
						});
					}
					$scope.setCurrentSection($scope.defaultFilterValues.section || "overview");
					$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_WIDGETS + 1;
				})
				.error(loadingHttpError);
		}, function(errResponse) {
			console.error(errResponse);
		});
	};

	var setCurrentSectionInternal = function(section_name) {
		var sub_secs = section_name.split('.');
		var section = '';
		$scope.currentSection = section_name;
		$scope.breadcrumb = [];
		for (var i = 0; i < sub_secs.length; i++) {
			var key = sub_secs[i];
			if (i != 0) {
				section += '.';
			}
			section += key;
			$scope.breadcrumb.push({
				key: key,
				section: section,
				is_last: (i == (sub_secs.length - 1))
			});
		}
	};

	$scope.setCurrentSection = function(section_name) {
		if (!$scope.sections[section_name]) {
			console.error("Section \"" + section_name + "\" not found!");
			return;
		}

		var default_chart = null;
		if ($scope.selectCharts01 && $scope.selectCharts01.extraParam.change_section && $scope.selectCharts01.extraParam.change_section.target) {
			default_chart = $scope.selectCharts01.extraParam.change_section.target;
		}
		setCurrentSectionInternal(section_name);
		$scope.charts = $scope.sections[section_name].charts;
		if ($scope.charts.length) {
			var default_chart_pos = -1;
			if ($scope.defaultFilterValues.chart) {
				default_chart_pos = findItemByKeyValue($scope.charts, "key", $scope.defaultFilterValues.chart, -1);
			}
			if (default_chart && default_chart_pos == -1) {
				default_chart_pos = findItemByKeyValue($scope.charts, "key", default_chart, -1);
			}
			if (default_chart_pos == -1) {
				default_chart_pos = 0;
			}
			$scope.selectCharts01 = $scope.charts[default_chart_pos];
		}
	};

	$scope.getCurrentSectionAtLevel = function(level) {
		if ($scope.currentSection) {
			var sub_secs = $scope.currentSection.split('.');
			if (sub_secs && level < sub_secs.length) {
				var ret = '';
				for (var i = 0; i <= level; i++) {
					if (i != 0) {
						ret += '.';
					}
					ret += sub_secs[i];
				}
				return ret;
			}
		}
		return null;
	};

	$scope.loadMainWidget = function() {
		$scope.chartsDetails = [];
		$scope.extraParamName = null;
		$scope.selectExtraParam = null;
		$scope.chartData = []; /* erase the chart */

		if (!$scope.validFilters() || !$scope.selectCharts01) {
			return;
		}
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_MAIN_WIDGET;

		/* Series */
		$scope.series = null;
		if ($scope.selectCharts01.extraParam.series) {
			var series = $scope.selectCharts01.extraParam.series;
			var first = null;
			for (var key in series) {
				if (!first) first = key;
				series[key].label = key;
			}
			if (first) {
				$scope.series = series;
				$scope.currentSerie = first;
			}
		}

		/* Now load the data */
		$scope.loadMainWidgetData();
	};

	$scope.drawMainWidget = function() {
		$scope.widgetOptions = {};
		if ($scope.selectCharts01.type === 'dataTable') {
			var tableRowClick = null;
			var change_section_params = null;
			var widget_url = null;
			if ($scope.selectCharts01.extraParam.change_section && $scope.selectCharts01.extraParam.change_section.section) {
				change_section_params = $scope.selectCharts01.extraParam.change_section.params;
				tableRowClick = function(data, domElement, iDisplayIndex, iDisplayIndexFull) {
					$scope.extraFilters = {};
					if ($scope.selectCharts01.extraParam.change_section.params) {
						for (var p in $scope.selectCharts01.extraParam.change_section.params) {
							var param_name = $scope.selectCharts01.extraParam.change_section.params[p];
							for (var c in $scope.currentData.meta) {
								if (param_name == $scope.currentData.meta[c].name) {
									$scope.extraFilters[param_name] = data[c];
									break;
								}
							}
						}
					}
					if ($scope.extraFilters.server_name) {
						var new_server_id = findItemByKeyValue($scope.servers, "server_name", $scope.extraFilters.server_name, 0);
						if ($scope.servers[new_server_id]) {
							/* The change of the current database will trigger the other changes */
							$scope.selectServer01 = $scope.servers[new_server_id];
						}
					}
					if ($scope.extraFilters.database_name) {
						var new_database_id = findItemByKeyValue($scope.databases, "database_name", $scope.extraFilters.database_name, 0);
						if ($scope.databases[new_database_id]) {
							/* The change of the current database will trigger the other changes */
							$scope.selectDatabase01 = $scope.databases[new_database_id];
						}
					}
					$scope.setCurrentSection($scope.selectCharts01.extraParam.change_section.section);
					$scope.$apply();
				};
			}
			if ($scope.selectCharts01.extraParam.server_side_process) {
				widget_url = databaseService.getWidgetURL($scope.getDatabaseInfo(), $scope.extraFilters, $scope.extraParamName,$scope.selectExtraParam, 'dataTableServerSide');
			}
			$scope.widgetOptions = {
				rowClickCallback: tableRowClick,
				serverSideProcessingURL: widget_url,
				subType: $scope.selectCharts01.extraParam.subType
			};
		}
	};

	function loadingHttpError(data, status, headers, config) {
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADED_ALL;
		$scope.currentData = [];
		$scope.widgetError = {"message": 'HTTP ' + status + (HTTP_STATUS_NAME[status] ? ' - ' + HTTP_STATUS_NAME[status] : '')};
	}

	$scope.loadMainWidgetData = function() {
		var hasExtraParam = $scope.extraParamName !== null && $scope.selectExtraParam;
		if ($scope.validFilters() && $scope.selectCharts01) {
			$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_MAIN_WIDGET;

			var dbInfo = $scope.getDatabaseInfo();

			databaseService
				.getWidgetData(
					dbInfo,
					$scope.extraFilters,
					$scope.extraParamName,
					$scope.selectExtraParam,
					$scope.selectCharts01.extraParam.server_side_process ? 'dataTableServerSide' : null
				)
				.success(function(data, status, headers, config) {
					$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_MAIN_WIDGET + 1;
					$scope.currentData = data;
					$scope.drawMainWidget();
				})
				.error(loadingHttpError);
		}
	}

	$scope.validFilters = function() {
		return $scope.selectServer01 &&
			$scope.selectDatabase01 &&
			$scope.dateFrom &&
			$scope.dateTo;
	}

	$scope.openPicker = function($event, picker) {
		$event.preventDefault();
		$event.stopPropagation();
		$scope[picker] = true;
	}

	function joinDateHour(date, hour) {
		if (!date || !hour) {
			return null;
		}
		return new Date(date.getFullYear(), date.getMonth(), date.getDate(), hour.getHours(), hour.getMinutes(), hour.getSeconds(), hour.getMilliseconds());
	}

	$scope.getDatabaseInfo = function() {
		var extraParam = $scope.selectCharts01 && $scope.selectCharts01.extraParam ? $scope.selectCharts01.extraParam : undefined;
		var _from = joinDateHour($scope.dateFrom, $scope.hourFrom);
		var _to = joinDateHour($scope.dateTo, $scope.hourTo);
		if (_from) _from = _from.toISOString();
		if (_to) _to = _to.toISOString();

		return {
			'customer': $scope.selectCustomer01 ? $scope.selectCustomer01.customer_id : null,
			'server': $scope.selectServer01 ? $scope.selectServer01.server_id : null,
			'instance': $scope.selectInstance01 ? $scope.selectInstance01.instance_id : null,
			'database': $scope.selectDatabase01 ? $scope.selectDatabase01.database_id : null,
			'schema': $scope.selectCustomer01 ? $scope.selectCustomer01.schema : null,
			'from': _from,
			'to': _to,
			'extraParam': extraParam && extraParam.query ? extraParam.query : undefined,
			'chart' : $scope.selectCharts01 ? $scope.selectCharts01.key : undefined
		};
	}

	function timeMoveRelativeToCurrent(hours_diff) {
		var dateTimeFrom = joinDateHour($scope.dateFrom, $scope.hourFrom);
		var dateTimeTo = joinDateHour($scope.dateTo, $scope.hourTo);
		$scope.dateFrom = $scope.hourFrom = new Date(dateTimeFrom.getTime() + (hours_diff * (1000 * 60 * 60)));
		$scope.dateTo = $scope.hourTo = new Date(dateTimeTo.getTime() + (hours_diff * (1000 * 60 * 60)));
		$scope.loadMainWidget();
	}

	$scope.timeMoveRelativeToEnd = function(hours_diff) {
		var dateTimeTo = $scope.maxDateTime;
		if (dateTimeTo) {
			$scope.dateFrom = $scope.hourFrom = new Date(dateTimeTo.getTime() + (hours_diff * (1000 * 60 * 60)));
			$scope.dateTo = $scope.hourTo = dateTimeTo;
			$scope.loadMainWidget();
		}
	}

	$scope.timeMoveBack = function() {
		timeMoveRelativeToCurrent(-1);
	};

	$scope.timeMoveForward = function() {
		timeMoveRelativeToCurrent(1);
	};

	$scope.filterDates = function() {
		var dbInfo = $scope.getDatabaseInfo();
		$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_DATETIME;
		databaseService
			.getDates(dbInfo)
			.success(function(data, status, headers, config) {
				var minDate = new Date(data.rows[0][0]);
				var maxMinusEight = new Date(data.rows[0][1]);
				var maxDate = new Date(data.rows[0][2]);

				$scope.minDateTime = minDate;
				$scope.maxDateTime = maxDate;

				var defaultFrom = $scope.defaultFilterValues.from;
				var defaultTo = $scope.defaultFilterValues.to;
				if (defaultFrom && defaultFrom.match(/^[0-9]+$/)) {
					defaultFrom = new Date(parseInt(defaultFrom));
				}
				if (defaultTo && defaultTo.match(/^[0-9]+$/)) {
					defaultTo = new Date(parseInt(defaultTo));
				}

				$scope.dateFrom = $scope.hourFrom = new Date(defaultFrom || maxMinusEight);
				$scope.dateTo = $scope.hourTo = new Date(defaultTo || maxDate);
				$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_DATETIME + 1;
			})
			.error(loadingHttpError);
	}

	$scope.$watch('breadcrumb', function(newvalue, oldvalue, scope) {
		if (scope.breadcrumb && scope.breadcrumb.length > 0) {
			var title = '';
			for (var b in scope.breadcrumb) {
				if (title) title += ' - ';
				var caption = $scope.i18n[scope.breadcrumb[b].section];
				if (caption && caption.title) {
					title += $interpolate(caption.title)(scope.extraFilters).replace(/<[^>]*>/g, '');
				} else {
					title += scope.breadcrumb[b].section;
				}
			}
			document.title = title + ' - pgAnalytics';
			return;
		}
		document.title = 'pgAnalytics';
	});

	function getURLParams() {
		var ret = $scope.getDatabaseInfo();
		ret.section = $scope.currentSection;
		if (countObjectKeys($scope.extraFilters) > 0) {
			ret.filters = JSON.stringify($scope.extraFilters);
		}
		return ret;
	}

	$(window).on('hashchange', function() {
		var url = window.location.hash;
		if (url.match(/^#/)) url = url.substr(1);
		if (url != $scope.currentURL) {
			var params = parseURLQuery(url);
			var savedParams = parseURLQuery($scope.currentURL);
			var paramsChanged = (countObjectKeys(params) != countObjectKeys(savedParams));
			/**
			 * Some browsers don't give the URL the same as encodeURIComponent
			 * (used on saveURL function), so we must check the de-serialized data
			 */
			if (!paramsChanged) {
				for (var i in savedParams) {
					if (savedParams[i] !== params[i]) {
						paramsChanged = true;
						break;
					}
				}
			}
			if (paramsChanged) {
				/* TODO: Validate the URL */
				$scope.defaultFilterValues = params;
				/* Reload everything again */
				if (params.filters) {
					$scope.extraFilters = JSON.parse(params.filters);
				}
				$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_CUSTOMER;
				if (params.section) $scope.setCurrentSection(params.section);
				$scope.$apply();
			}
		}
	});

	$scope.saveURL = function() {
		var dbInfo = getURLParams();
		var ret = "";
		if ($scope.defaultFilterValues.currentSection && $scope.defaultFilterValues.currentSection !== $scope.currentSection) {
			/* Force to change current section */
			$scope.setCurrentSection($scope.defaultFilterValues.currentSection);
			return;
		}
		blink_chartsbtn_cnt = 0;
		for (var i in dbInfo) {
			if (dbInfo[i] !== undefined && dbInfo[i] !== null && !$scope.ignoreKeysForURL[i]) {
				if (ret.length == 0) {
					ret += "?";
				} else {
					ret += "&";
				}
				ret += encodeURIComponent(i) + "=" + encodeURIComponent(dbInfo[i]);
			}
		}
		$scope.currentURL = ret;
		window.location.hash = ret;

		GA.sendEvent(
			/* category */ $scope.selectCustomer01.name_id,
			/* action */   ($scope.selectCharts01 && $scope.selectCharts01.type ? $scope.selectCharts01.type : 'null'),
			/* label */    dbInfo.section + ' ' + dbInfo.chart
			);
	};

	function installMainNavigationWatches() {
		for (var i = 0; i < $scope.mainNavigation.stepMeta.length; i++) {
			var meta = $scope.mainNavigation.stepMeta[i];
			if (meta.currentVar) {
				(function(currentVar, step) {
					$scope.$watch(currentVar, function(newValue, oldValue, scope) {
						if (scope.mainNavigation.currStep > step+1) {
							/* If a var changes, we can go back in the steps, but we can't move forward (as it needs to reach the step one by one) */
							scope.mainNavigation.currStep = step+1;
						}
					});
				})(meta.currentVar, i);
			}
		}
	}

	$scope.$watch('mainNavigation.currStep', function(newValue, oldValue, scope) {
		if (
			(newValue != oldValue || newValue >= scope.mainNavigation.LOADED_ALL)
			&& scope.mainNavigation.stepMeta[newValue]
			&& scope.mainNavigation.stepMeta[newValue].loadCallback
		) {
			if (newValue < scope.mainNavigation.LOADED_ALL) {
				scope.widgetError = null;
			}
			//console.log("moving to step ", newValue, ": ", scope.mainNavigation.stepMeta[newValue].name);
			scope.mainNavigation.stepMeta[newValue].loadCallback();
		}
	});

	$scope.$watchCollection('[hourFrom, hourTo, dateFrom, dateTo, selectCharts01]', function(newValues, oldValues, scope) {
		if (scope.mainNavigation.currStep == scope.mainNavigation.LOADED_ALL) {
			scope.mainNavigation.currStep = scope.mainNavigation.LOADING_MAIN_WIDGET;
		}
	});

	function checkHourWrap(newValue, oldValue, scope, varName) {
		/**
		 * It is an ugly hack, when the user is at 00:* and move one hour less, to
		 * 23:*, (through the timestamp component) it does move the date component,
		 * the same when it is at 23:* and moves up to 00:*.
		 * XXX: The caveat here is that the user may do it manually (without pressing
		 * the down/up arrow).
		 * */
		if (newValue && oldValue) {
			var mv = 0;
			if (newValue.getHours() == 23 && oldValue.getHours() == 0) {
				mv = -1;
			} else if (newValue.getHours() == 0 && oldValue.getHours() == 23) {
				mv = 1;
			}
			if (mv != 0) {
				var d = new Date(scope[varName]);
				d.setDate(d.getDate() + mv);
				scope[varName] = d;
			}
		}
	}

	$scope.$watch('hourFrom', function(newValue, oldValue, scope) {
		checkHourWrap(newValue, oldValue, scope, 'dateFrom');
	});

	$scope.$watch('hourTo', function(newValue, oldValue, scope) {
		checkHourWrap(newValue, oldValue, scope, 'dateTo');
	});

	$scope.loadWidgetHelpDetails = function() {
		$scope.widgetHelpDetails = null;
		if ($scope.selectCharts01 && $scope.selectCharts01.key && $scope.i18n[$scope.selectCharts01.key] && $scope.i18n[$scope.selectCharts01.key].has_help) {
			databaseService
				.getWidgetHelp('pt_BR', $scope.selectCharts01.key)
				.then(function(response) {
					if (response.data && response.data.length === 1) {
						$scope.widgetHelpDetails = $sce.trustAsHtml(response.data[0].help);
					}
				}, function(errResponse) {
					console.error(errResponse);
					$scope.widgetHelpDetails = errResponse.toString();
				});
		} else {
			$scope.widgetHelpDetails = '...';
		}
	};

	/* TODO: Refactoring on the highlight logic (this is really a simple PoC) */
	var blink_chartsbtn_cnt = 0;
	var blink_filter_cnt = 0;
	function blinkBtnCharts() {
		var blink_class = 'blink';
		if (blink_filter_cnt >= 12) {
			if (blink_chartsbtn_cnt < 6) {
				if (blink_chartsbtn_cnt % 2 == 0) {
					$scope.btnChartsHighlight = blink_class;
				} else {
					$scope.btnChartsHighlight = '';
				}
				$scope.$apply();
				blink_chartsbtn_cnt++;
				return;
			}
		} else {
			if (blink_filter_cnt % 2 == 0) {
				$scope.btnFilterHighlight = blink_class;
			} else {
				$scope.btnFilterHighlight = '';
			}
			$scope.$apply();
			blink_filter_cnt++;
			return;
		}
		$scope.btnChartsHighlight = '';
		$scope.btnFilterHighlight = '';
	}
	setInterval(blinkBtnCharts, 500);

	$scope.initMain();

}]);

})(angular);

