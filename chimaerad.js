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

var editor = undefined;
var session = undefined;

var connected = true; // is the app connected to the daemon?
var interfaces = undefined;
var devices = undefined;
var device = undefined;
var apis = undefined;
var ports = undefined;

var tab = 'menu_disc';

function update_menu() {
	$('#tab_disc').hide();
	$('#tab_midi').hide();
	$('#tab_conf').hide();

	if(tab == 'menu_disc')
		$('#tab_disc').show();
	else if(tab == 'menu_midi')
		$('#tab_midi').show();
	else if(tab == 'menu_conf')
		$('#tab_conf').show();
}

function update_interfaces(data) {
	var items = [];
	if(data.success == false)
		return;
	interfaces = data.reply;
	if(interfaces === undefined)
		return;

	$.each(interfaces, function(i, iface) {
		items.push('<li>' + iface + '</li>')	
	});
	$('#interfaces').html(items.join());
}

function update_devices(data) {
	var items = [];
	if(data.success == false)
		return;
	devices = data.reply;
	if(devices === undefined)
		return;

	$.each(devices, function(i, device) {
		items.push('<li><input type="radio" name="devices_radio" value="' + device + '">' + device + '</input></li>');
	});
	$('#devices').html(items.join());
	$('input[type="radio"][name="devices_radio"]').click(on_click_device);
}

function update_apis(data) {
	var items = [];
	if(data.success == false)
		return;
	apis = data.reply;
	if(apis === undefined)
		return;

	$.each(apis, function(i, api) {
		items.push('<li><input type="radio" name="apis_radio" value="' + api + '">' + api + '</input></li>');
	});
	$('#apis').html(items.join());
	$('input[type="radio"][name="apis_radio"]').click(on_click_apis);

	// update ports from that API
	var query = {'query':{'/chimaerad/ports':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_ports);
}

function update_ports(data) {
	var items = [];
	if(data.success == false)
		return;
	ports = data.reply;
	if(ports === undefined)
		return;

	var j = 0;
	$.each(ports, function(i, port) {
		items.push('<li><input type="radio" name="ports_radio" value="' + i + '">' + port + '</input></li>');
		j++;
	});
	items.push('<li><input type="radio" name="ports_radio" value="-1">Virtual</input></li>');
	$('#ports').html(items.join());
	$('input[type="radio"][name="ports_radio"]').click(on_click_ports);
}

function on_menu(e) {
	tab = $(this).attr('id');
	update_menu();
}

function on_change_name(e) {
	device.name = $(this).val();
}

function on_change_lease(e) {
	device.lease = $(this).val();
	$('#device_ip').prop('value', device.ip).prop('disabled', device.lease != 'static').change(on_change_ip);
}

function on_change_ip(e) {
	device.ip = $(this).val();
}

function on_change_mode(e) {
	device.mode = $(this).val();
}

function on_change_rate(e) {
	device.rate = parseInt($(this).val(), 10);
};

function on_change_sync(e) {
	device.sync = $(this).val();
};

function update_device(data) {
	if(data.success == false)
		return;
	device = data.reply;

	if(device === undefined) {
		$('#device').hide();
		return;
	}
	$('#div_comm').show();

	// comm
	$('#device_uid').html(device.uid);
	$('#device_name').val(device.name).change(on_change_name);
	$('#device_ip_dhcp').prop('checked', device.lease == 'dhcp').change(on_change_lease);
	$('#device_ip_ipv4ll').prop('checked', device.lease == 'ipv4ll').change(on_change_lease);
	$('#device_ip_static').prop('checked', device.lease == 'static').change(on_change_lease);
	$('#device_ip').prop('value', device.ip).prop('disabled', device.lease != 'static').change(on_change_ip);
	if(device.reachable == false) {
		$('#device_reachable').html('is <b>not</b> reachable, please change the lease mode or static IP to match your network interface(s).');
		$('.device_reach').hide();
		return;
	}
	$('#device_reachable').html('is reachable');
	$('.device_reach').show();

	// claim
	$('#device_claim').prop('checked', device.claimed);
	if(device.claimed == false) {
		$('#div_conf').hide();
		return;
	}
	$('#div_conf').show();

	// config
	$('#device_mode_udp').prop('checked', device.mode == 'udp').change(on_change_mode);
	$('#device_mode_tcp').prop('checked', device.mode == 'tcp').change(on_change_mode);

	$('#device_rate_1000').prop('checked', device.rate == 1000).change(on_change_rate);
	$('#device_rate_1500').prop('checked', device.rate == 1500).change(on_change_rate);
	$('#device_rate_2000').prop('checked', device.rate == 2000).change(on_change_rate);
	$('#device_rate_2500').prop('checked', device.rate == 2500).change(on_change_rate);
	$('#device_rate_3000').prop('checked', device.rate == 3000).change(on_change_rate);

	$('#device_sync_none').prop('checked', device.sync == 'none').change(on_change_sync);
	$('#device_sync_sntp').prop('checked', device.sync == 'sntp').change(on_change_sync);
	$('#device_sync_ptp').prop('checked',  device.sync == 'ptp' ).change(on_change_sync);
}

// keep connection between browser and daemon alive
function keepalive() {
	var json = {'query':{'/chimaerad/keepalive':{}}};
	$.getJSON('/?'+JSON.stringify(json), function(data) {
		// TODO handle event(s)
	}).done(function() {
		$('#page').show();
		$('#connection_state').html('v0.1.0 - <b>Connected</b> to daemon.');
		if(!connected)
			location.reload(); // FIXME only used for development
		connected = true;
	}).fail(function() {
		$('#page').hide();
		$('#connection_state').html('v0.1.0 - Connection to daemon <b>FAILED</b>, please check whether the daemon is still up and running.');
		connected = false;
	});

	setTimeout(keepalive, 1000);
}

// get all discovered devices
function on_click_devices(e) {
	var query = {'query':{'/chimaerad/sources':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_devices);
}

// get info of device with given UID and make it the active device
function on_click_device(e) {
	var uid = $(this).val();
	var query = {'query':{'/chimaerad/source/info':{'uid':uid}}};
	$.getJSON('/?'+JSON.stringify(query), update_device);
}

// get info of Midi APIs with given name and make it the active API
function on_click_apis(e) {
	var name = $(this).val();
	var query = {'query':{'/chimaerad/api/claim':{'name':name}}};
	$.getJSON('/?'+JSON.stringify(query), update_apis);
}

// get info of Midi port with given name and make it the active port
function on_click_ports(e) {
	var pos = parseInt($(this).val(), 10);
	var query = {'query':{'/chimaerad/port/claim':{'pos':pos}}};
	$.getJSON('/?'+JSON.stringify(query), update_ports);
}

// get com of active device
function on_device_com(e) {
	var query = {'query':{'/chimaerad/source/com':device}};
	$('#device').hide();
	$.getJSON('/?'+JSON.stringify(query), update_device);
}

// (un)claim active device
function on_device_claim(e) {
	var claim = {'query':{'/chimaerad/source/claim':device}};
	device.claimed = $(this).prop('checked');
	$('#div_conf').hide();
	$.getJSON('/?'+JSON.stringify(claim), update_device);
}

// get conf of active device
function on_device_conf(e) {
	var query = {'query':{'/chimaerad/source/conf':device}};
	$('#device').hide();
	$.getJSON('/?'+JSON.stringify(query), update_device);
}

// send code from editor to daemon
function on_device_code(e) {
	var code = editor.getValue();
	var query = {'query':{'/chimaerad/source/code':code}};
	$('#device').hide();
	$.getJSON('/?'+JSON.stringify(query), null);
}

function on_code_load(data) {
	editor.setValue(data);
	editor.gotoLine(1);
	editor.resize();
}

/*
 * run after page load/refresh
 */
$(document).ready(function() {
	var query;

	// grab interfaces
	query = {'query':{'/chimaerad/interfaces':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_interfaces);
	
	query = {'query':{'/chimaerad/apis':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_apis);

	$.get('/dummy.lua', on_code_load);

	$('#connection_state').show();
	$('#device_com').unbind().click(on_device_com);
	$('#device_claim').unbind().change(on_device_claim);
	$('#device_conf').unbind().click(on_device_conf);
	$('#device_code').unbind().click(on_device_code);
	
	$('#menu_disc').unbind().click(on_menu);
	$('#menu_midi').unbind().click(on_menu);
	$('#menu_conf').unbind().click(on_menu);
	update_menu();

	on_click_devices(null); //TODO add <a>

	// start keepalive loop
	keepalive();
	
	editor = ace.edit('editor');
	editor.setTheme('ace/theme/clouds_midnight');
	editor.setShowPrintMargin(false);
	session = editor.getSession();
	session.setMode('ace/mode/lua');
	session.setTabSize(2);
});
