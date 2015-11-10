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
		})
		.when('/calibration/:device', {
			templateUrl: 'calibration.html',
			controller: 'calibrationController'
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

app.controller('calibrationController', function($scope, $route, $timeout, $http) {
	var device_name = $route.current.params.device;

	var s = [];
	for(var i=0; i<160; i++) {
		s[i] = Math.random();
	}
	$scope.sensors = s;

	$scope.onTimeout = function() {
		$http.post('/api/v1/sensors', {url: device_name})
			.success(function(data) {
				$scope[data.key] = data.value;
				mytimeout = $timeout($scope.onTimeout, 40);
			})
	}
	var mytimeout = $timeout($scope.onTimeout, 40);
});

app.directive("sensorDump", function () {
	return {
		restrict: 'E',
		scope: {
			sensors: '='
		},
		template: "<canvas width='1280' height='320' />",
		link: function(scope, element, attrs) {
			console.log(element);
			scope.canvas = element.find('canvas')[0];
			scope.context = scope.canvas.getContext('2d');

			scope.$watch('sensors', function(newValue) {
				var context = scope.context;
				var width = scope.canvas.width;
				var height = scope.canvas.height;
				var barWidth = width / newValue.length;
				var barWidth2 = barWidth - 2;
				var m = height / 2;
				var s = m / 2048;
				var thresh0 = 2048 / m;
				var thresh1 = 2032;

				// fill background
				context.fillStyle = '#222';
				context.fillRect(0, 0, width, height);

				// set line parameters
				context.strokeStyle = '#fff';
				context.lineWidth = 1;
				context.setLineDash([]);

				// draw solid lines
				//context.strokeRect(0, 0, width, height);
				context.beginPath();
					context.moveTo(0, m);
					context.lineTo(width, m);
				context.stroke();
				context.closePath();
			
				context.beginPath();
					context.moveTo(0, height);
					context.lineTo(width, height);
				context.stroke();
				context.closePath();

				context.beginPath();
					context.moveTo(0, 0);
					context.lineTo(width, 0);
				context.stroke();
				context.closePath();

				// draw dashed lines
				context.setLineDash([1, 4]);

				for(var i=0; i<=160; i+=16) {
					var x = i*barWidth;
					context.beginPath();
						context.moveTo(x, 0);
						context.lineTo(x, height);
					context.stroke();
					context.closePath();
				}

				// draw sensor bars
				for(var i=0; i<newValue.length; i++) {
					var val = newValue[i];
					var vala = Math.abs(val)
					if(vala < thresh0)
						continue;

					var rel = vala * s;
					context.fillStyle = vala >= thresh1 ? '#bb0' : '#b00';
					context.fillRect(barWidth*i + 1, val >= 0 ? m : m-rel, barWidth2, rel);
				}
			});
		}
	}
});

app.controller('deviceController', function($scope, $http, $route) {
	var device_name = $route.current.params.device;
	$scope.uri = $route.current.params.uri;

	$scope.devices = null;
	$scope.device = null;
	$scope.items = {};
	$scope.contents = null;

	var success = function(data) {
		console.log(data);
		$scope[data.key] = data.value;
		$scope.device = $scope.devices[device_name];

		$http.post('/api/v1/query', {url: device_name, path: $scope.uri})
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
		$http.post('/api/v1/query', {url: device_name, path: $scope.uri + item})
			.success(function(data) {
				$scope.items[item] = data.value;
			}); //TODO .error
	}

	$scope.get = function(item) {
		var arg = $scope.items[item].arguments[0];

		$http.post('/api/v1/get', {url: device_name, path: $scope.uri + item})
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
			url: device_name,
			path: $scope.uri + item,
			format: arg.type,
			value: arg.value
		}).success(function(data) {
			}); //TODO .error
	}

	$scope.call = function(item) {
		$http.post('/api/v1/call', {url: device_name, path: $scope.uri + item})
			.success(function(data) {
			}); //TODO .error
	}
});
