pganalytics = angular.module('pganalytics',['nvd3', 'ui.bootstrap', 'pganalytics.service']);

pganalytics.factory('pgaHttpInterceptor', ['$q', function($q) {
	return {
		request: function(config) {
			return config
		},
		response: function(response) {
			return response
		},
		requestError : function(request) {
			return $q.reject(request);
		},
		responseError : function(response) {
			if(response.status === 401) {
				window.location.href = "/pga/login.html";
			} else {
				return $q.reject(response);
			}
		}
	}
}]);

pganalytics.config(['$httpProvider', function($httpProvider) {
	$httpProvider.interceptors.push();
}]);

var serviceModule = angular.module('pganalytics.service', [ 'ng' ]);

serviceModule.service('databaseService', function($http, $q){
	return new DatabaseService($http, $q);
});

serviceModule.service('i18nService', function($http, $q){
	return new i18nService($http, $q);
});

pganalytics.controller('analiseSqlCtrl', function ($scope, $http, $modal) {
		//TODO Refact na forma que insere a tabela;
}).directive('analiseSql', function() {
	return {
		templateUrl: 'analise-sql.html'
	};
});

pganalytics.filter('quote_literal', function() {
	return function(input) {
		return PQQuoteLiteral(input);
	};
});

pganalytics.directive('i18n', ['$interpolate', function($interpolate) {
	return {
		restrict: 'A',
		link: function(scope, element, attr) {
			var originalValue = element.text();
			function updateElement(newValue) {
				var value;
				if (!newValue || !newValue[attr.i18n]) {
					value = {
						title: originalValue || attr.i18n,
						tooltip: ''
					};
				} else {
					var type = attr.i18nType || 'title';
					value = {
						title: newValue[attr.i18n][type] || attr.i18n,
						tooltip: newValue[attr.i18n].tooltip
					};
				}
				if (attr.i18nParams) {
					attr.$$element.html($interpolate(value.title)(scope.$eval(attr.i18nParams)));
				} else {
					attr.$$element.text(value.title);
				}
				if (attr.i18nNoTooltip === undefined) {
					attr.$$element.attr('title', value.tooltip);
				}
			}
			attr.$observe('i18n', function(newValue) {
				updateElement(scope.i18n);
			});
			scope.$watch('i18n', function(newValue, oldValue) {
				updateElement(newValue);
			});
			attr.$observe('i18nParams', function(newValue) {
				updateElement(scope.i18n);
			});
			if (attr.i18nParams) {
				scope.$watch(attr.i18nParams, function(newValue, oldValue) {
					updateElement(scope.i18n);
				}/*, true */);
			}
		}
	};
}]);

pganalytics.directive('pgaGrid', [function() {
	return {
		restrict: 'A',
		link: function(scope, element, attr) {
			//data-row="{{c.grid.row}}" data-col="{{c.grid.col}}" data-sizex="{{c.grid.sizex}}" data-sizey="{{c.grid.sizey}}" 
			var grid = scope.$eval(attr.pgaGrid);
			for (var i in grid) {
				element.attr('data-' + i, grid[i]);
			}
			if (scope.$last === true) {
				element.parent().parent().addClass('gridster');
				var gridsterOptions = {
					widget_base_dimensions: [400, 200],
					widget_margins: [5, 5],
					auto_init: true
					
				};
				if (attr.pgaGridDragHandler) {
					gridsterOptions.draggable = {
						handle: attr.pgaGridDragHandler
					};
				}
				gridster = $(element.parent().get(0)).gridster(gridsterOptions).data('gridster');
			}
		}
	};
}]);

pganalytics.directive('pgaWidthResizeWatcher', [function() {
	return {
		restrict: 'E',
		scope: {
			callback: '&',
			delay: '=?'
		},
		link: function(scope, element, attr) {
			var css = {
				border: '0',
				display: 'block',
				width: '100%',
				height: '0'
			};
			var $parent = $(element.parent());
			var $iframe =
				$('<iframe border=0></iframe>')
					.prependTo($parent);
			$iframe.css(css);
			var lastWidth = null;
			var lastTimeout = null;
			var checkNewWidth = function() {
				lastTimeout = null;
				var newWidth = $($iframe.get(0).contentWindow).width();
				if (lastWidth === newWidth) {
					scope.callback();
					scope.$apply();
				}
			};
			if (isNaN(attr.delay)) {
				/* By default, we use delay of 5 */
				attr.delay = 5;
			}
			$($iframe.get(0).contentWindow).on('resize', function() {
				if (attr.delay && !isNaN(attr.delay)) {
					var newWidth = $(this).width();
					if (lastWidth === newWidth)
						return;
					lastWidth = newWidth;
					if (lastTimeout) {
						clearTimeout(lastTimeout);
					}
					lastTimeout = setTimeout(checkNewWidth, attr.delay);
				} else {
					scope.callback();
				}
			});
		}
	};
}]);

