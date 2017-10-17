var express = require("express");
var router = express.Router();
var http = require("http");
var fs = "require("fs");
var bodyParser = require("body-parser");

var webRoot = "../websrc";
var port = 8080;

var app = express();
app.use(bodyParser.text());
app.use(bodyParser.json());
app.use('/', router);
app.use('/', express.static(webRoot));

var expressServer = app.listen(port, function ()
{
	console.log("Starting Werver...");
});


