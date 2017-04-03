function GenericService($http, $q, path) {

	this.baseUrl = '/pga/s/sql';

	this.promise = function(request) {
		return GenericService.promise(request, $q);
	};
};

GenericService.extend = function(target, $http, $q, path) {
	$.extend(target, new GenericService($http, $q, path));
};

GenericService.promise = function(request, $q) {
	var deferred = $q.defer();

	request.success(function(result) {
		deferred.resolve(result);
	}).error(function(result) {
		deferred.reject(result);
	});

	return deferred.promise;
};	