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

$.postJSON = function(data) {
	var dup = data;

	if(typeof(dup.data) == 'object')
		dup.data = JSON.stringify(dup.data);
	else if(typeof(dup.data) == 'string')
		dup.data = dup.data;
	else
		return;

	dup.type = 'POST';
	dup.dataType = 'json';

	$.ajax(dup);
}

var devices = {}
var device = undefined;
var connected = true;

$.postOSC = function(data) {
	$.postJSON({
		url: '/?',
		data: data,
		error: function(jqXHR, err) {
			console.log(err);
		},
		success: function(data, status) {
			console.log(JSON.stringify(data));
		}
	});
}

function devices_change(e) {
	//TODO
	var name = $('#devices_name').prop('value');
	var ipv4ll = $('#devices_txt_claim_ipv4ll').prop('checked');
	var dhcp = $('#devices_txt_claim_dhcp').prop('checked');
	var static = $('#devices_txt_claim_static').prop('checked');
	var address = $('#devices_address').prop('value');

	console.log('devices_change', name, ipv4ll, dhcp, static, address);

	$.postOSC({
		'request': '/devices/change',
		'data': {
			'target': device.fullname,
			'name': name,
			'ipv4ll': ipv4ll ? 1 : null,
			'dhcp': dhcp ? 1 : null,
			'static': static ? 1 : null,
			'address': address
		}
	});
};

function header_toggle(e) {
	var id = $(this).attr('id');
	
	$('#discovery').hide();
	$('#configuration').hide();
	$('#communication').hide();
	$('#event_handling').hide();

	if(id == 'menu_discovery')
		$('#discovery').show();
	else if(id == 'menu_configuration')
		$('#configuration').show();
	else if(id == 'menu_communication')
		$('#communication').show();
	else if(id == 'menu_event_handling')
		$('#event_handling').show();
};

function devices_toggle(e) {
	var key = $(this).attr('id');
	device = devices[key];

	if(!device)
		return;

	$('#devices_name').prop('value', device.name);
	$('#devices_port').html(device.port);

	$('#devices_txt_reset').html(device.txt.reset);
	$('#devices_txt_claim_ipv4ll').prop('checked', device.txt.ipv4ll && !device.txt.dhcp)
	$('#devices_txt_claim_dhcp').prop('checked', device.txt.dhcp);
	$('#devices_txt_claim_static').prop('checked', device.txt.static);
	$('#devices_address').prop('value', device.address);
	$('#devices_address').prop('disabled', !device.txt.static);
	$('#devices_reachable').html(device.reachable ? 'yes' : 'no (you need to reconfigure the device IP)');
	$('#devices_change').off('click').on('click', devices_change);
	
	$('.devices').show();
	
	$('#comms_mode_udp').prop('checked', device.mode == 'osc.udp');
	$('#comms_mode_tcp').prop('checked', device.mode == 'osc.tcp');

	if(device.reachable)
		$('.comms').show();
	else
		$('.comms').hide();
}

function api_v1_keepalive() {
	if(!connected) {
		$('#daemon_status').html('disconnected (<i>check whether the "chimaerad" daemon is up and running and then refresh this page)</i>.');

		$('.lists').hide();
		devices = {};
		$('.devices').hide();
		$('.comms').hide();
		return;
	} else {
		$('#daemon_status').html('connected');
		$('.lists').show();
	}

	$.postJSON({
		url:'/api/v1/keepalive',
		data:null,
		timeout:60000,
		error:function(jqXHR, status) {
			if(status == 'error')
				connected = false;
		},
		success:function(reply, status) {
			if(status != 'success' || reply.status != 'success') {
				console.log('api_v1_keepalive' + reply.message);
				return;
			}
			getAPI(reply.data.href);
		},
		complete:api_v1_keepalive
	});
};

function api_v1_interfaces(reply, status) {
	if(status != 'success' || reply.status != 'success') {
		console.log('api_v1_interfaces' + reply.message);
		return;
	}

	var itm = [];
	$.each(reply.data, function(i, conf) {
		if(!conf.internal && (conf.version == 'inet'))
			itm.push(conf.name + ' (' + conf.address + ')');
	});
	$('#ifaces_list').html(itm.join(', '));
}

function api_v1_devices(reply, status) {
	if(status != 'success' || reply.status != 'success') {
		console.log('api_v1_devices' + reply.message);
		return;
	}

	var itm = [];
	$.each(reply.data, function(key, conf) {
		devices[key] = conf;
		itm.push('<a href="#" id="' + key + '">' + key + '</a>');
	});
	$('#devices_list').html(itm.join(', '));
	$.each(reply.data, function(key, conf) {
		$('a[id="'+key+'"]').click(devices_toggle);
	});
	$('.devices').hide();
	$('.comms').hide();
}

var api_v1_get = {
	'/api/v1/interfaces': api_v1_interfaces,
	'/api/v1/devices': api_v1_devices
};

function postAPI(url, data) {
	$.postJSON({url:url, data:data, success:api_v1_post[url]});
};

function getAPI(url) {
	$.getJSON(url, null, api_v1_get[url]);
};

$(document).ready(function() {
	api_v1_keepalive();

	getAPI('/api/v1/interfaces');
	getAPI('/api/v1/devices');
	
	$('a.header').off('click').on('click', header_toggle);
});
