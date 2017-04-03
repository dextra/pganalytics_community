(function(angular) {
'use strict';

var pganalytics = angular.module('pganalytics');
pganalytics.controller('DashboardCtrl', ['$scope', '$sce', '$interpolate', '$controller', '$interval', 'databaseService', 'i18nService', function($scope, $sce, $interpolate, $controller, $interval, databaseService, i18nService) {
	/* DashboardCtrl inherits MainCtrl */
	$controller("MainCtrl", {
			"$scope": $scope,
			"$sce": $sce,
			"$interpolate": $interpolate,
			"$controller": $controller,
			"databaseService": databaseService,
			"i18nService": i18nService
		});

	$scope.initDashboard = function() {
		$scope.initMain();
		$scope.chartsData = [];
		$scope.widgets = {};
		$scope.refreshIntervalDelayMin = 5;
		/* Those items are non-sense in the dashboard's URL, so just make the MainCtrl ignore those: */
		$scope.ignoreKeysForURL.from = $scope.ignoreKeysForURL.to = $scope.ignoreKeysForURL.chart = $scope.ignoreKeysForURL.section = true;
		/* Install the refresher interval */
		if ($scope.defaultFilterValues.refresh) {
			var aux = parseInt($scope.defaultFilterValues.refresh);
			if (!isNaN(aux)) {
				$scope.refreshIntervalDelayMin = aux;
			}
		}
		$scope.refreshIntervalStart();
	}

	$scope.$watch('mainNavigation.currStep', function(newValue, oldValue, scope) {
		console.debug(newValue, scope.mainNavigation.stepMeta[newValue].name);
		if (newValue === $scope.mainNavigation.LOADING_WIDGETS + 1) {
			$scope.widgets = {};
			/* After loading the widgets/sections, load each widget apart into $scope.widgets */
			for (var i in $scope.sections) {
				for (var j in $scope.sections[i].charts) {
					var chart = $scope.sections[i].charts[j];
					if (!$scope.widgets[chart.key]) {
						$scope.widgets[chart.key] = chart;
					}
				}
			}
		}
	});

	$scope.loadMainWidgetData = function() {
		var hasExtraParam = $scope.extraParamName !== null && $scope.selectExtraParam;
		if ($scope.validFilters() && $scope.selectCharts01) {
			$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_MAIN_WIDGET;

			var display_graphs = [
				{
					key: "dashboard_cpuusage",
					grid: {
						row: 1,
						col: 1,
						sizex: 1,
						sizey: 1
					}
				},
				{
					key: "sostats_cpuusage",
					grid: {
						row: 1,
						col: 2,
						sizex: 1,
						sizey: 2
					}
				},
				{
					key: "sostats_cpuload",
					serie: "load_avg",
					grid: {
						row: 1,
						col: 3,
						sizex: 1,
						sizey: 2
					}
				},
				{
					key: "dashboard_diskusage",
					grid: {
						row: 2,
						col: 1,
						sizex: 1,
						sizey: 1
					}
				},
				{
					key: "overview_alerts_by_server",
					class: "start-line",
					grid: {
						row: 3,
						col: 1,
						sizex: 1,
						sizey: 4
					}
				},
				{
					key: "pgstats_checkpoints",
					serie: "checkpoint_by",
					grid: {
						row: 3,
						col: 2,
						sizex: 1,
						sizey: 2
					}
				},
				{
					key: "pgstats_globalcacheratio",
					grid: {
						row: 3,
						col: 3,
						sizex: 1,
						sizey: 2
					}
				},
				{
					key: "sostats_memusage",
					serie: "memory_usage",
					grid: {
						row: 4,
						col: 2,
						sizex: 1,
						sizey: 2
					}
				},
				{
					key: "overview_alerts",
					grid: {
						row: 4,
						col: 3,
						sizex: 1,
						sizey: 2
					}
				}
			];
			$scope.chartsData = [];
			var promises = [];
			$scope.chartsLoadedCount = 0;
			for (var i in display_graphs) {
				var widgetConfig = display_graphs[i];
				var dbInfo = $scope.getDatabaseInfo();
				dbInfo.chart = widgetConfig.key;
				$scope.chartsData.push({
					idx: parseInt(i),
					key: widgetConfig.key,
					grid: widgetConfig.grid,
					serie: widgetConfig.serie,
					class: widgetConfig.class || 'default',
					data: null,
					widget: $scope.widgets[widgetConfig.key],
					api: null
				});
				(function(i, dbInfo, widgetConfig) {
					var chartConfig = $scope.widgets[widgetConfig.key];
					databaseService
						.getWidgetData(
							dbInfo,
							{}   /*$scope.extraFilters*/,
							null /*$scope.extraParamName*/,
							null /*$scope.selectExtraParam*/,
							null
						)
						.then(function(response) {
							/* Set the data and options to the view */
							$scope.chartsData[i].data = response.data;
							$scope.chartsLoadedCount++;
							if ($scope.chartsLoadedCount == display_graphs.length) {
								$scope.mainNavigation.currStep = $scope.mainNavigation.LOADING_MAIN_WIDGET + 1;
							}
						});
				})(i, dbInfo, widgetConfig);
			}
			$scope.widgetError = null;
		}
	}

	$scope.refreshIntervalCallback = function() {
		var idx;
		/* Force to always reload the values */
		$scope.defaultFilterValues.from
			= $scope.defaultFilterValues.to
			= $scope.defaultFilterValues.customer
			= $scope.defaultFilterValues.server
			= $scope.defaultFilterValues.instance
			= null;
		/* Next instance */
		idx = findItemByKeyValue($scope.instances, "instance_id", $scope.selectInstance01 && $scope.selectInstance01.instance_id, $scope.instances.length);
		if (idx >= 0 && idx < $scope.instances.length - 1) {
			$scope.selectInstance01 = $scope.instances[idx + 1];
		} else {
			/* No instance, next server */
			idx = findItemByKeyValue($scope.servers, "server_id", $scope.selectServer01 && $scope.selectServer01.server_id, $scope.servers.length);
			if (idx >= 0 && idx < $scope.servers.length - 1) {
				$scope.selectServer01 = $scope.servers[idx + 1];
			} else {
				/* No server, next customer */
				idx = findItemByKeyValue($scope.customers, "customer_id", $scope.selectCustomer01 && $scope.selectCustomer01.customer_id, $scope.customers.length);
				if (idx >= 0 && idx < $scope.customers.length - 1) {
					$scope.selectCustomer01 = $scope.customers[idx + 1];
				} else {
					/* Wrap */
					console.debug('wrap');
					if ($scope.customers.length > 1) {
						$scope.selectCustomer01 = $scope.customers[0];
					} else if ($scope.servers.length > 1) {
						$scope.selectServer01 = $scope.servers[0];
					} else if ($scope.instances.length > 1) {
						$scope.selectInstance01 = $scope.instances[0];
					} else {
						/* Only one customer, one server, and one instance (not uncommon). Reload the date/time. */
						$scope.filterDates();
					}
				}
			}
		}
	};

	$scope.refreshIntervalStart = function() {
		if ($scope.refreshIntervalRunning) {
			$scope.refreshIntervalStop();
		}
		$scope.refreshIntervalRunning = true;
		$scope.refreshInterval = $interval($scope.refreshIntervalCallback, $scope.refreshIntervalDelayMin * 60000);
	};

	$scope.refreshIntervalStop = function() {
		$scope.refreshIntervalRunning = false;
		$interval.cancel($scope.refreshInterval);
	};

	$scope.initDashboard();

}]);

})(angular);

