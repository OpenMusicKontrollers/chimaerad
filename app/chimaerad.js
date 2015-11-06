/*
 * Copyright (c) 2015 Hanspeter Portner (dev@open-music-kontrollers.ch)
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the Artistic License 2.0 as published by
 * The Perl Foundation.
 *
 * This source is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * Artistic License 2.0 for more details.
 *
 * You should have received a copy of the Artistic License 2.0
 * along the source as a COPYING file. If not, obtain it from
 * http://www.perlfoundation.org/artistic_license_2_0.
 */

var app = angular.module('chimaeraD', ['ngRoute']);

app.config(function($routeProvider, $httpProvider) {
	$routeProvider
		.otherwise({
			redirectTo: '/'
		})
		.when('/', {
			templateUrl: 'main.html',
			controller: 'mainController'
		})
		.when('/device/:device/:uri*', {
			templateUrl: 'device.html',
			controller: 'deviceController'
		});

    $httpProvider.defaults.timeout = 5000;
});

app.controller('mainController', function($scope, $http) {
	$scope.connected = true;
	$scope.interfaces = null;
	$scope.devices = null;

	var success = function(data) {
		console.log(data);
		$scope[data.key] = data.value;
	}

	var error = function(data) {
		console.log('error');
	}

	$scope.keepalive = function() {
		$http.post('/api/v1/keepalive', {}, {timeout:3.6e6}) // 1 hour
			.success(function(data) {
				$scope.connected = true;
				success(data);
				$scope.keepalive();
			})
			.error(function(data) {
				$scope.connected = false;
				error(data);
			});
	}

	$scope.api = function(path) {
		$http.post(path, {})
			.success(success)
			.error(error);
	}
});

app.controller('deviceController', function($scope, $http, $route) {
	var device = $route.current.params.device;
	var path = $route.current.params.uri;
	$scope.uri = path;

	$scope.devices = null;
	$scope.device = null;
	$scope.url = null;
	$scope.items = {};
	$scope.contents = null;

	var success = function(data) {
		console.log(data);
		$scope[data.key] = data.value;
		$scope.device = $scope.devices[device];
		$scope.url = $scope.device.fullname.replace(/([^.]+)._osc._([^.]+).local/g,
			'osc.$2' + ($scope.device.version == 'inet' ? '4' : '6') + '://$1.local:' + $scope.device.port);
	
		$http.post('/api/v1/query', {url: $scope.url, path: $scope.uri})
			.success(function(data) {
				$scope[data.key] = data.value;
			}); //TODO .error
	}

	var error = function(data) {
		console.log($scope.uri);
	}

	$scope.api = function(path) {
		$http.post(path, {})
			.success(success)
			.error(error);
	}

	$scope.query = function(item) {
		$http.post('/api/v1/query', {url: $scope.url, path: path+item})
			.success(function(data) {
				$scope.items[item] = data.value;
			}); //TODO .error
	}

	$scope.get = function(item) {
		var arg = $scope.items[item].arguments[0];

		$http.post('/api/v1/get', {url: $scope.url, path: path+item})
			.success(function(data) {
				var value = data.value;
				if(arg.type == 'i' && arg.range && arg.range[0] == 0 && arg.range[1] == 1) {
					value = value === 1 ? true : false;
				}
				$scope.items[item].arguments[0].value = value;
			}); //TODO .error
	}

	$scope.set = function(item) {
		var arg = $scope.items[item].arguments[0];

		$http.post('/api/v1/set', {
			url: $scope.url,
			path: path+item,
			format: arg.type,
			value: arg.value
		}).success(function(data) {
			}); //TODO .error
	}

	$scope.call = function(item) {
		$http.post('/api/v1/call', {url: $scope.url, path: path+item})
			.success(function(data) {
			}); //TODO .error
	}
});
