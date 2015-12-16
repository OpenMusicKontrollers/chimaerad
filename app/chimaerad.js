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
			redirectTo: '/interfaces'
		})
		.when('/interfaces', {
			templateUrl: 'interfaces.html',
			controller: 'interfaceController'
		})
		.when('/ports', {
			templateUrl: 'ports.html',
			controller: 'portController'
		})
		.when('/device/:device//calibration/', {
			templateUrl: 'calibration.html',
			controller: 'calibrationController'
		})
		.when('/device/:device/:uri*', {
			templateUrl: 'device.html',
			controller: 'deviceController'
		});

    $httpProvider.defaults.timeout = 5000;
});

app.controller('mainController', function($scope, $http) {
	$scope.version = '0.1.0';
	$scope.connected = true;
	$scope.interfaces = null;
	$scope.ports = null;
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

app.controller('interfaceController', function($scope) {
	//TODO
});

app.controller('portController', function($scope) {
	//TODO
});

app.controller('calibrationController', function($scope, $route, $timeout, $http) {
	var device_name = $route.current.params.device;
	$scope.state = 0;
	$scope.sensor = null;
	$scope.value = [null, null, null, 0.25, 0.5, 0.75, null, 0.75, 0];

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
	
	$scope.call = function(item, state) {
		console.log(item, state, $scope.value[$scope.state]);
		if($scope.value[$scope.state]) {
			var fmt = item == 'save' ? 'i' : 'f';
			$http.post('/api/v1/set', {url: device_name, path: '/calibration/' + item, format: fmt, value: $scope.value[$scope.state]})
				.success(function(data) {
					if(data.value)
						$scope.sensor = data.value;
				}); //TODO .error
		} else {
			$http.post('/api/v1/call', {url: device_name, path: '/calibration/' + item})
				.success(function(data) {
				}); //TODO .error
		}
		$scope.state = state;
	}

	$scope.abort = function() {
		$httpd.post('/api/v1/set', {url: device_name, path: '/calibration/end', format: 'f', value: 1.0})
			.success(function(data) {
			}); //todo .ERROR
		$httpd.post('/api/v1/set', {url: device_name, path: '/calibration/load', format: 'i', value: 0})
			.success(function(data) {
			}); //todo .ERROR
		$scope.state = 0;
	};
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

	$scope.device = $scope.devices[device_name]; // inherited from parent scope
	$scope.items = {};
	$scope.contents = null;

	$scope.intro = function() {
		$http.post('/api/v1/query', {url: device_name, path: $scope.uri})
			.success(function(data) {
				$scope[data.key] = data.value;
			}); //TODO .error
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
