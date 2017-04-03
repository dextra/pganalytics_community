var pganalytics = angular.module('pganalytics', ['ng']);

function LoginCtrl($scope) {
	$scope.init = function() {
		var params = parseURLQuery();
		if (params.errmsg) {
			$scope.errmsg = i18n[params.errmsg] || params.errmsg;
		}
		if (params.infomsg) {
			$scope.infomsg = i18n[params.infomsg] || params.infomsg;
		}
	}
}
