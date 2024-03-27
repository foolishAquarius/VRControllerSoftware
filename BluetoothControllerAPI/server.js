var fs = require('fs');
var path = require('path');

//Default configuration
var config = {
  spm: { }, //Serial port manager
  cli: true, //Command line interface
  port: 5147,
  mode: "http", //HTTP server with WebSocket by default
  prefix: "/api/v1", //REST API route prefix, e.g. "/" for root or "/api/v1" etc.
  verbose: process.env.NODE_VERBOSE == "true" || process.env.NODE_VERBOSE == "1",
  debug: process.env.NODE_DEBUG == "true" || process.env.NODE_DEBUG == "1", //Even more details than verbose
  permissions: {
    list: true,
    read: true,
    write: true,
    ui: true,
    ws: true
  }
};

function startWebServer(config) {
  var express = require('express');
  //var logger = require('morgan');
  var engine = require('ejs');
  var bodyParser = require('body-parser');
  
  //Initialize express
  var app = express();
  app.startup = new Date();
  app.uptime = function() {
    return Math.ceil(new Date().getTime() - app.startup.getTime());
  };

  //Set up the view engine
  app.set('views', path.join(__dirname, 'views'));
  app.set('view engine', 'ejs');

  //app.use(logger('dev'));

  //Register path /lib/RemoteSerialPort.js for web access
  var clientPath = path.join(__dirname, "lib");
  if (fs.existsSync(clientPath)) {
    app.use("/lib", express.static(clientPath));
  }
  else {
    if (config.permissions.ui) {
      console.warn("Warning: Web interface not available!");
    }
    config.permissions.ui = false;
  }

  //Default index page
  app.get("/", function(req, res, next) {
    if (!config.permissions.ui) {
      return next(new Error("No web interface permissions!"));
    }
    res.render("index");
  });

  //API status and version
  app.get(config.prefix, function(req, res, next) {
    var pkg = require('./package.json');
    res.json({ name: pkg.name, version: pkg.version, uptime: app.uptime() });
  });

  //Register REST API
  var webserver = require('./lib/webserver.js');
  var subapp = webserver(config);
  app.use(config.prefix, subapp);

  //Catch 404 and forward to error handler
  app.use(function(req, res, next) {
    var err = new Error('Not Found');
    err.status = 404;
    next(err);
  });

  //Error handler
  app.use(function(err, req, res, next) {
    if (config.verbose) {
      console.error(err);
    }
    
    //HTTP status code
    res.status(err.status || 500);
    
    if (!config.permissions.ui) {
      return res.end();
    }
    
    //HTML output
    res.render('error', {
      status: err.status,
      message: err.message,
      stack: err.stack
    });
  });
  
  //Start the HTTP server
  var server = app.listen(config.port, function() {
    console.log('HTTP on port ' + server.address().port);
  });

  //WebSocket
  if (config.permissions.ws) {
    try {
      var websocket = require('./lib/websocket.js');
      var wss = websocket(config).use(server);
      
      //Register receive event to forward data over web socket
      if (config.spm && typeof config.spm == "object") {
        config.spm.on("received", function(e) {
          var port = config.spm[e.port];
          if (!port || !port.websockets) {
            return;
          }
          var websockets = port.websockets;
          if (websockets && config.permissions.read) {
            for (var i = 0; i < websockets.length; i++) {
              var ws = websockets[i];
              ws.send(e.data);
            }
          }
        });
      }
    }
    catch (error) {
      console.warn("Warning: WebSocket not available!", error);
    }
  }
}

startWebServer(config);