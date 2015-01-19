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

var connected = true;
var events = {
	'/dns_sd/browse': function(data, status) {
		console.log('dns_sd event');
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
			console.log(JSON.stringify(data));
			if(data.success) {
				var reply = data.reply;
				var cb = events[reply.request];
				if(cb !== undefined)
					cb(reply, status);
			};
		}
	});
}

function keepalive() {
	//console.log('keepalive');

	if(!connected)
	{
		$('#daemon_status').html('disconnected (<i>check whether the "chimaerad" daemon is up and running and then refresh this page)</i>.');
		return;
	}
	else
		$('#daemon_status').html('connected');

	$.postJSON({
		url: '/?',
		data: {'request': '/keepalive'},
		timeout: 10*1000,
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
	update({'request': '/dns_sd/browse'});
});
