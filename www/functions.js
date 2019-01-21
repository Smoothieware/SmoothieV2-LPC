var wsUri = "ws://192.168.1.101";
//var wsUri = "ws://demos.kaazing.com";
var websocket;
function connectCommand() {
	if(websocket && websocket.readyState == 1) {
		websocket.close();
		return;
	}

	$( "#connectionStatus" ).empty().append("Connecting...");
	//ws = new WebSocket("ws://demos.kaazing.com/echo");
	websocket = new WebSocket(wsUri + "/command");
	websocket.onopen = function(evt) { onOpen(evt) };
	websocket.onclose = function(evt) { onClose(evt) };
	websocket.onmessage = function(evt) { onMessage(evt) };
	websocket.onerror = function(evt) { onError(evt) };
}

function displayConnectionStatus(arg) {
  	$( "#connectionStatus" ).empty().append(arg);
}

function onOpen(evt)
{
	displayConnectionStatus("CONNECTED");
	// FIXME this does not work... why?
 	$('#ConnectButton').text("Disconnect")
}

function onClose(evt)
{
	displayConnectionStatus("DISCONNECTED");
	$('#ConnectButton').text("Connect")
}

function onMessage(evt)
{
	$( "#result" ).empty();
	$.each(evt.data.split('\n'), function(index) {
		$( "#result" ).append( this + '<br/>' );
	});
}

function onError(evt)
{
  displayConnectionStatus('Error:' + evt.data);
}

function doSend(message)
{
  websocket.send(message);
}

function runCommand(cmd, silent) {
	// TODO implement silent
  //url = silent ? "/command_silent" : "/command"; // $form.attr( "action" );
  // Send the data using post
  doSend(cmd);
}

function runCommandSilent(cmd) {
  runCommand(cmd, true);
}

function runCommandCallback(cmd,callback) {
	// TODO how to do this?
	//var posting = $.post( url, cmd, callback);
}

function jogXYClick (cmd) {
  runCommand("G91 G0 " + cmd + " F" + document.getElementById("xy_velocity").value + " G90", true)
}

function jogZClick (cmd) {
  runCommand("G91 G0 " + cmd + " F" + document.getElementById("z_velocity").value + " G90", true)
}

function extrude(event,a,b) {
  var length = document.getElementById("extrude_length").value;
  var velocity = document.getElementById("extrude_velocity").value;
  var direction = (event.currentTarget.id=='extrude')?1:-1;
  runCommand("G91 G0 E" + (length * direction) + " F" + velocity + " G90", true);
}

function motorsOff(event) {
  runCommand("M18", true);
}

function heatSet(event) {
  var type = (event.currentTarget.id=='heat_set')?104:140;
  var temperature = (type==104)?document.getElementById("heat_value").value:document.getElementById("bed_value").value;
  runCommand("M" + type + " S" + temperature, true);
}

function heatOff(event) {
  var type = (event.currentTarget.id=='heat_off')?104:140;
  runCommand("M" + type + " S0", true);
}
function getTemperature () {
  runCommand("M105", false);
}

function handleFileSelect(evt) {
	var files = evt.target.files; // handleFileSelectist object

	// files is a FileList of File objects. List some properties.
	var output = [];
	for (var i = 0, f; f = files[i]; i++) {
		output.push('<li><strong>', escape(f.name), '</strong> (', f.type || 'n/a', ') - ',
			f.size, ' bytes, last modified: ',
			f.lastModifiedDate ? f.lastModifiedDate.toLocaleDateString() : 'n/a',
			'</li>');
	}
	document.getElementById('list').innerHTML = '<ul>' + output.join('') + '</ul>';
}

function upload() {
	$( "#progress" ).empty();
	$( "#uploadresult" ).empty();
	$( "#uploadError" ).empty();

	if(websocket && websocket.readyState != 3) {
	  websocket.close();
	}
	//ws = new WebSocket("ws://demos.kaazing.com/echo");
	ws = new WebSocket(wsUri + "/upload");
	ws.binaryType = "arraybuffer";

	ws.onopen = function() {
		$( "#progress" ).empty().append("Connected.")
		// take the file from the input
		var file = document.getElementById('files').files[0];
		var reader = new FileReader();
		reader.readAsBinaryString(file); // alternatively you can use readAsDataURL

		var rawData = new ArrayBuffer();
		reader.loadend = function() { $( "#progress" ).empty().append("the File has been loaded.")}
		reader.onload = function(e) {
		  console.log("Uploading file: " + file.name + ", length: " + rawData.byteLength);
		  rawData = e.target.result;
		  ws.send(file.name);
		  ws.send(rawData.byteLength);
		  for (var i = 0; i < rawData.byteLength; i+=1024) {
			if(i+1024 <= rawData.byteLength) {
				//console.log("sending: " + i + " - " + (i + 1024));
				ws.send(rawData.slice(i, i+1024));
			}else{
				//console.log("sending: " + i + " - " + (rawData.byteLength - i));
				ws.send(rawData.slice(i));
			}
		  }
		  //$( "#progress" ).empty().append("the File has been transferred.")
		};
	};

	ws.onmessage = function(evt) {
		//$( "#uploadresult" ).empty().append(evt.data);
		$( "#uploadresult" ).append(evt.data);
	};

	ws.onclose = function(e) {
		$( "#progress" ).empty().append("Connection is closed..." + e.code + " " + e.reason);
	};

	ws.onerror = function(e) {
		$( "#uploadError" ).empty().append("There was an ERROR");
	}

}

function playFile(filename) {
  runCommandSilent("play /sd/"+filename);
}

function refreshFiles() {
  document.getElementById('fileList').innerHTML = '';
  runCommandCallback("M20", function(data){
	$.each(data.split('\n'), function(index) {
	  var item = this.trim();
		if (item.match(/\.g(code)?$/)) {
		  var table = document.getElementById('fileList');
		  var row = table.insertRow(-1);
		  var cell = row.insertCell(0);
		  var text = document.createTextNode(item);
		  cell.appendChild(text);
		  cell = row.insertCell(1);
		  cell.innerHTML = "[<a href='javascript:void(0);' onclick='playFile(\""+item+"\");'>Play</a>]";
		}
		//$( "#result" ).append( this + '<br/>' );
	  });
  });
}
