(function(pganalytics) {

"use strict";

pganalytics.directive('pgaWidget', [function() {
	return {
		restrict: 'E',
		scope: {
			data: '=',
			widget: '=',
			options: '=',
			i18n: '=i18nObject',
			api: '=?'
		},
		templateUrl: 'templates/pgaWidgetDirective.html'
	};
}]);

pganalytics.directive('pgaChart', [function() {

	function defaultChartOptions(data, widget, currentSerie, options) {
		var overrideOptions = {};
		var ret = {
			chart: {
				type: widget.type,
				height: 450,
				margin : {
					top: 20,
					right: 60,
					bottom: 60,
					left: 60
				},
				x: function(d){return d[0];},
				y: function(d){return d[1];},
				useVoronoi: false,
				clipEdge: true,
				transitionDuration: 500,
				useInteractiveGuideline: true,
				xAxis: {
					tickFormat: function(d) {
						if (d == Math.floor(d)) { /* Checks if it is an integer value */
							if (options && options.context === 'dashboard') {
								return i18n.formatter.chartTime(new Date(data.rows[d][0]));
							} else {
								return i18n.formatter.chartDateTime(new Date(data.rows[d][0]));
							}
						} else {
							return "";
						}
					},
					staggerLabels: true
				},
				yAxis: {
					tickFormat: function(d){
						var unit = widget.extraParam.unit || 'float';
						if (currentSerie && widget.extraParam.series && widget.extraParam.series[currentSerie].unit) {
							unit = widget.extraParam.series[currentSerie].unit;
						}
						return i18n.formatter.numericUnit(d, unit);
					}
				},
				color: function(d, i) {
					var colors = ["#1f77b4", "#ff7f0e", "#2ca02c", "#d62728", "#9467bd", "#8c564b", "#e377c2", "#7f7f7f", "#bcbd22", "#17becf"];
					return d.color || colors[i % colors.length];
				},
				noData: i18n.noDataFound || 'No data found',
				controlsData: ["Stacked", "Expanded"],
				controlLabels: {
					stacked: i18n.chart_stacked || 'Stacked',
					expanded: i18n.chart_expanded || 'Expanded'
				}
			}
		};
		if (options && options.context === 'dashboard') {
			ret.chart.margin = {
				top: 5,
				right: 20,
				bottom: 20,
				left: 60
			};
			ret.chart.height = null;
			ret.chart.controlsData = [];
			ret.chart.interpolate = 'monotone';
			ret.chart.showLegend = false;
			ret.chart.xAxis.staggerLabels = false;
		}
		return ret;
	}

	return {
		restrict: 'E',
		templateUrl: 'templates/pgaChartDirective.html',
		scope: {
			data: '=',
			widget: '=',
			options: '=',
			i18n: '=i18nObject',
			api: '=?'
		},
		controller: function($scope) {
			$scope.tableApi = {};
			$scope.chartOptions = undefined;
			function reloadWidget() {
				var serieKeys = null;
				if (
					!$scope.data
					|| !$scope.widget
					|| (
						$scope.widget.extraParam
						&& $scope.widget.extraParam.series
						&& (
							!$scope.currentSerie
							|| !$scope.widget.extraParam.series[$scope.currentSerie]
						)
					)
				) {
					$scope.chartdata = [];
					return;
				}
				$scope.display = 'chart';
				if ($scope.widget.extraParam && $scope.widget.extraParam.series && $scope.widget.extraParam.series[$scope.currentSerie]) {
					serieKeys = $scope.widget.extraParam.series[$scope.currentSerie].keys;
				}
				$scope.chartData = gerarDados($scope.data, serieKeys);
				$scope.chartOptions = defaultChartOptions($scope.data, $scope.widget, $scope.currentSerie, $scope.options);
				if ($scope.api) {
					$scope.api.resize = function() {
						if ($scope.display === 'chart' && $scope.chartApi && $scope.chartApi.refresh) {
							$scope.chartApi.refresh();
						} else if ($scope.display === 'table' && $scope.tableApi && $scope.tableApi.resize) {
							$scope.tableApi.resize();
						}
					};
				}
			}
			$scope.$watchCollection('[data, currentSerie]', reloadWidget);
			$scope.$watch('widget', function(newValue, oldValue) {
				if (newValue && newValue.extraParam && newValue.extraParam.series) {
					if ($scope.options && $scope.options.serie) {
						$scope.currentSerie = $scope.options.serie;
					} else {
						/* Get first key */
						for (var i in newValue.extraParam.series) {
							$scope.currentSerie = i;
							break;
						}
					}
				}
			});
			$scope.$watch('display', function(newValue, oldValue) {
				if (newValue) {
					if (newValue === 'chart' && $scope.chartApi && $scope.chartApi.refresh) {
						setTimeout($scope.chartApi.refresh, 1);
					} else if (newValue == 'table' && $scope.tableApi && $scope.tableApi.refresh) {
						setTimeout($scope.tableApi.refresh, 1);
					}
				}
			});
		},
		link: function(scope, element, attr) {
			element.find('.save-chart').on('click', function() {
				if (scope.display == 'chart')
					saveChartImage(element.find('.widget-container svg'));
				else if (scope.display == 'table' && scope.tableApi)
					saveTableCSV(scope.tableApi.tableElement(), element.find('.save-chart-loading'));
			});
		}
	};
}]);

pganalytics.directive('pgaTable', [function() {
	return {
		restrict: 'E',
		templateUrl: 'templates/pgaTableDirective.html',
		scope: {
			data: '=',
			widget: '=',
			options: '=',
			i18n: '=i18nObject',
			api: '=?'
		},
		controller: function($scope) {
			function reloadWidget() {
				if (!$scope.data) {
					return;
				}
				if (!$scope.options) {
					$scope.options = {};
				}
				var processed_data = parseJsonToTable(
					$scope.data,
					($scope.context === 'dashboard' || $scope.widget.extraParam.sortable === false),
					$scope.widget.extraParam.attr_defs || {}
				);
				if ($scope.options.subType === "sql") {
					mountDataTableForSQLCommands(
						processed_data,
						$scope.containerElement,
						$scope.widget.key
						);
				} else {
					var rowClickCallback = $scope.options.rowClickCallback;
					if ($scope.widget.extraParam.row_click_callback) {
						var callbacks = $scope.widget.extraParam.row_click_callback;
						/* TODO: Create a more appropriated callback register */
						rowClickCallback = function(aData, nRow, iDisplayIndex, iDisplayIndexFull) {
							if ($scope.options.rowClickCallback) {
								if (!$scope.options.rowClickCallback.apply(this, [aData, nRow, iDisplayIndex, iDisplayIndexFull])) {
									return false;
								}
							}
							for(var c in callbacks) {
								var callback = callbacks[c];
								if (callback && callback.command && typeof window[callback.command] === "function") {
									window[callback.command].apply(this, [callback.args, processed_data, aData, nRow, iDisplayIndex, iDisplayIndexFull]);
								} else {
									console.error("Invalid callback: ", callback);
								}
							}
						};
					}
					mountDataTableExhibition(
						processed_data,
						$scope.containerElement,
						rowClickCallback,
						$scope.options.serverSideProcessingURL,
						$scope.options.extraDataTableOptions
						);
				}
			}
			$scope.$watchCollection('[data]', reloadWidget);
			if ($scope.api) {
				$scope.api.resize = $scope.api.refresh = function() {
					var $tbl = $scope.containerElement.data('dataTableComponent');
					if ($tbl) {
						$tbl.dataTable().fnAdjustColumnSizing();
					}
				};
				$scope.api.tableElement = function() {
					var $tbl = $scope.containerElement.data('dataTableComponent');
					if ($tbl) {
						return $tbl;
					}
				};
			}
		},
		link: function(scope, element, attr) {
			var $containerElement = element.find('.table-container');
			scope.containerElement = $containerElement;
			element.find('.save-chart').on('click', function() {
				saveTableCSV($containerElement.data('dataTableComponent'), element.find('.save-chart-loading'));
			});
			element.on('$destroy', function() {
				var $tbl = $containerElement.data('dataTableComponent');
				if ($tbl) {
					$tbl.dataTable().fnDestroy();
				}
			});
		}
	};
}]);

pganalytics.directive('pgaGauge', [function() {

	var opts = {
		lines: 12, // The number of lines to draw
		angle: 0, // The length of each line
		lineWidth: 0.37, // The line thickness
		pointer: {
			length: 0.85, // The radius of the inner circle
			strokeWidth: 0.055, // The rotation offset
			color: '#000000' // Fill color
		},
		limitMax: false,   // If true, the pointer will not go past the end of the gauge
		colorStart: '#40CF7E',   // Colors
		//colorStop: '#CCCCCC',    // just experiment with them
		strokeColor: '#666666',   // to see which ones work best for you
		generateGradient: true,
		percentColors: [[0.0, "#33ff33" ], [0.7, "#f9c802"], [0.9, "#ff0000"]]
	};

	return {
		restrict: 'E',
		templateUrl: 'templates/pgaGaugeDirective.html',
		scope: {
			data: '=',
			widget: '=',
			options: '=',
			i18n: '=i18nObject',
			api: '=?'
		},
		controller: function($scope) {
			function reloadWidget() {
				if (!$scope.data || !$scope.data.rows || $scope.data.rows.length !== 1) {
					return;
				}
				var unit = $scope.widget.extraParam && $scope.widget.extraParam.unit || 'float';
				var val = $scope.data.rows[0][0];
				var max = $scope.data.rows[0][1];
				$scope.gauge.maxValue = max;
				$scope.gauge.set(val);
				$scope.displayText = i18n.formatter.numericUnit(val, unit);
			}
			$scope.$watchCollection('[data]', reloadWidget);
			if ($scope.api) {
				$scope.api.resize = $scope.api.refresh = function() {
				};
				$scope.api.gauge = function() {
					return scope.gauge;
				};
			}
		},
		link: function(scope, element, attr) {
			var $containerElement = element.find('.gauge-container');
			var $canvas = $('<canvas></canvas>').appendTo($containerElement);
			var gauge = new Gauge($canvas.get(0)).setOptions(opts); // create sexy gauge!
			scope.containerElement = $containerElement;
			scope.canvas = $canvas;
			scope.gauge = gauge;

			//gauge.maxValue = 3000; // set max gauge value
			//gauge.animationSpeed = 32; // set animation speed (32 is default value)
			//gauge.set(1775); // set actual value
		}
	};
}]);

})(pganalytics);

