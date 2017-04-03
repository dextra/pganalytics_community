var pganalytics = angular.module('pganalytics', ['ng']);

function ResetPasswordCtrl($scope) {
	$scope.init = function() {
		var params = parseURLQuery();
		$scope.token = params.token || '';
		$scope.username = params.username || '';
	}
}
