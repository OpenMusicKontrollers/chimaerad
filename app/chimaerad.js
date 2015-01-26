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

	return $.ajax(dup);
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

var events = {
	'/ifaces/list': function(data, status) {
		var itm = [];
		$.each(data, function(i, conf) {
			if(!conf.internal && (conf.version == 'inet'))
				itm.push(conf.name + ' (' + conf.address + ')');
		});
		$('#ifaces_list').html(itm.join(', '));
	},

	'/dns_sd/browse': function(data, status) {
		var itm = [];
		$.each(data, function(key, conf) {
			devices[key] = conf;
			itm.push('<a href="#" id="' + key + '">' + key + '</a>');
		});
		$('#devices_list').html(itm.join(', '));
		$.each(data, function(key, conf) {
			$('a[id="'+key+'"]').click(devices_toggle);
		});
		$('.devices').hide();
		$('.comms').hide();
	}
}

function update(data) {
	//console.log('update', JSON.stringify(data));

	$.postJSON({
		url: '/?',
		data: data,
		error: function(jqXHR, err) {
			console.log(err);
		},
		success: function(data, status) {
			//console.log(JSON.stringify(data));
			if(data.success) {
				var reply = data.reply;
				var cb = events[reply.request];
				if(cb !== undefined)
					cb(reply.data, status);
			};
		}
	});
}

function keepalive() {
	//console.log('keepalive');

	if(!connected)
	{
		$('#daemon_status').html('disconnected (<i>check whether the "chimaerad" daemon is up and running and then refresh this page)</i>.');

		$('.lists').hide();
		devices = {};
		$('.devices').hide();
		$('.comms').hide();
		return;
	}
	else
	{
		$('#daemon_status').html('connected');

		$('.lists').show();
	}

	$.postJSON({
		url: '/?',
		data: {'request': '/keepalive'},
		timeout: 60*1000,
		complete: keepalive,
		error: function(jqXHR, err) {
			if(err == 'timeout')
				; //console.log(err);
			if(err == 'error')
				connected = false;
		},
		success: function(data, status) {
			console.log(JSON.stringify(data));
			if(data.success) {
				update(data.reply);
			}
		}
	});
};

$(document).ready(function() {
	keepalive();
	update({'request': '/ifaces/list'});
	update({'request': '/dns_sd/browse'});
});
