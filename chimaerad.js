var editor = undefined;
var session = undefined;

var connected = true; // is the app connected to the daemon?
var interfaces = undefined;
var devices = undefined;
var device = undefined;
var ports = undefined;

function update_interfaces(data) {
	var items = [];
	if(data.success == false)
		return;
	interfaces = data.reply;
	if(interfaces === undefined)
		return;

	$.each(interfaces, function(i, iface) {
		items.push('<tr><td><b>Network interface #' + (i+1) + '</b></td><td>' + iface + '</td></tr>')	
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
		items.push('<tr><td><b>Chimaera device #' + (i+1) + '</b></td><td><input type="radio" name="devices_radio" value="' + device + '">' + device + '</input></td></tr>');
	});
	$('#devices').html(items.join());
	$('input[type="radio"][name="devices_radio"]').click(on_click_device);
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
		items.push('<tr><td><b>Midi port #' + (i+1) + '</b></td><td><input type="radio" name="ports_radio" value="' + i + '">' + port + '</input></td></tr>');
		j++;
	});
	items.push('<tr><td><b>Midi port #' + (j+1) + '</b></td><td><input type="radio" name="ports_radio" value="-1">Virtual</input></td></tr>');
	$('#ports').html(items.join());
	$('input[type="radio"][name="ports_radio"]').click(on_click_ports);
}

function on_change_name(e) {
	device.name = $(this).val();
}

function on_change_lease(e) {
	device.lease = $(this).val();
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
	$('#device').show();

	// comm
	$('#device_caption').html(device.name);
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
		$('.device_config').hide();
		return;
	}
	$('.device_config').show();

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
	$('.device_config').hide();
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
	// grab interfaces
	var query = {'query':{'/chimaerad/interfaces':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_interfaces);

	query = {'query':{'/chimaerad/ports':{}}};
	$.getJSON('/?'+JSON.stringify(query), update_ports);

	$.get('/dummy.lua', on_code_load);

	$('#connection_state').show();
	$('#device_com').unbind().click(on_device_com);
	$('#device_claim').unbind().change(on_device_claim);
	$('#device_conf').unbind().click(on_device_conf);
	$('#device_code').unbind().click(on_device_code);

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
