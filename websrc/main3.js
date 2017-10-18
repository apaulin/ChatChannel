function hasUserMedia()
{
  return !!(navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia || navigator.msGetUserMedia);
}

function hasRTCPeerConnection()
{
	window.RTCPeerConnection = window.RTCPeerConnection || window.webkitRTCPeerConnection || window.mozRTCPeerConnection;
	
	return window.RTCPeerConnection;
}


var yourVideo =  document.querySelector('#yours');
var theirVideo = document.querySelector('#theirs');
var	 yourConnection;
var theirConnection;

if (hasUserMedia())
{
  navigator.getUserMedia = navigator.getUserMedia || navigator.webkitGetUserMedia || navigator.mozGetUserMedia || navigator.msGetUserMedia;
  navigator.getUserMedia({
    video: true,
    audio: false
    }, function (stream){
 
		yourVideo.src = window.URL.createObjectURL(stream);
		
		if (hasRTCPeerConnection())
		{
			startPeerConnection(stream);
		}
		else
		{
			alert("Browser don't support WebRTC");
		}
    }, function (err){ alert("No Camera")});
}
else
{
  alert("No support for getUserMedia");
}

var startPeerConnection = function(stream)
{
	var configuration = {
		iceServers : [ {url : "stun:127.0.0.1:9876"}] 
	};
	
	yourConnection = new RTCPeerConnection(configuration);
	theirConnection = new RTCPeerConnection(configuration);
	
	yourConnection.addStream(stream);
	yourConnection.onaddstream = function(e) 
	{
		theirVideo.src = window.URL.createObjectURL(e.stream);
	}
	
	
	yourConnection.onicecandidate = function (event)
	{
		if (event.candidate)
		{
			theirConnection.addIceCandidate(new RTCIceCandidate(event.candidate));
		}
	};

	theirConnection.onicecandidate = function (event)
	{
		if (event.candidate)
		{
			yourConnection.addIceCandidate(new RTCIceCandidate(event.candidate));
		}
	};

	yourConnection.createOffer(function (offer) {
		yourConnection.setLocalDescription(offer);
		theirConection.setRemoteDescription(offer);
		
		theirConnection.createAnswer(function(offer) {
			theirConnection.setLocalDescription(offer);
			yourConnection.setRemoteDescription(offer);
		});
	});
};


