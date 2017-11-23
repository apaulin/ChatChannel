console.log("Server started");
var Msg = '';
var WebSocketServer = require('ws').Server
var wss = new WebSocketServer({port: 8020});
var users = {};


wss.on("connection", function(connection) {
	console.log("User Connected");
	
	connection.on("message", function (message) {
		console.log("Got Message : " + message);
		var date;
		
		try
		{
			data = JSON.parse(message);
		} 
		catch (e)
		{
			data = {};
		}
		
		switch (data.type)
		{
			case "login":
				console.log("User logged in as", data.name);
				if (users[data.name])
				{
					sendTo(connection, {type: "login", success: true});
				}
				else
				{
					users[data.name] = connection;
					connection.name = data.name;
					sendTo(connection, {type: "login", success: true});
				}
				break;
			case "offer":
				console.log("Sending offer to: ", data.name);
				var conn = users[data.name];
				
				if (conn != null)
				{
					connection.otherName = data.name;
					sendTo(conn, {
						type: "offer",
						offer: data.offer,
						name: connection.name
					});
				}
				break;
			case "answer":
				console.log("Sending answer to", data.name);
				var conn = users[data.name];
				
				if (conn != null)
				{
					connection.otherName = data.name;
					sendTo(conn, {
						type: "answer",
						answer: data.answer
					});
				}
				break;
			case "candidate":
				console.log("Sending candidate to " , data.name);
				var conn = users[data.name];
				
				if (conn != null)
				{
					sendTo(conn, {
						type: "candidate",
						candidate: data.candidate
					});
				}
				break;
			case "leave":
				console.log("Disconnecting user from ", data.name);
				var conn = users[data.name];
				conn.otherName = null;
				
				if (conn != null)
				{
					sendTo(conn, {
						type: "leave"
					});
				}
				break;
			default:
				sendTo(connection, {type: "error", message: "Unrecognized command: " + data.type});
				break;
			
		}
	});
	
	connection.on("close", function() {
		if(connection.name)
		{
			delete users[connection.name];
			
			if(connection.otherName)
			{
				console.log("Disconnecting user from ", connection.otherName);
				var conn = users[connection.otherName];
				conn.otherName = null;
				
				if (conn != null)
				{
						sendTo(conn, {
							type: "leave"
						});
				}
			}
		}
	});
	
	//connection.send("Hello World");
});

 
var sendTo = function (conn, message)
{
	conn.send(JSON.stringify(message));
}
