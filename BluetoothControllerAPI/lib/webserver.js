let x = 512;
let y = 512;

const BluetoothSerialPort = require('bluetooth-serial-port');

const address = '98:D3:C1:FD:BF:53'; // Replace with the Bluetooth device address

const btSerial = new BluetoothSerialPort.BluetoothSerialPort();

let recvString = "";

let savedString = "";

btSerial.findSerialPortChannel(address, function (channel) {
  btSerial.connect(address, channel, function () {
      console.log('Connected to Bluetooth device');

      // Send data
      btSerial.write(Buffer.from('Start Polling'), function (err, bytesWritten) {
          if (err) console.error(err);
          console.log(`Sent ${bytesWritten} bytes`);

          // Receive data
          btSerial.on('data', function (buffer) {
              recvString += buffer.toString('utf-8');
              //recvBuff = Buffer.concat([recvBuff, buffer]);
              //const string = recvBuff.toString('utf-8');

              const newlineIndex = recvString.indexOf('\n');

              // If a newline is found, process and print the data
              if (newlineIndex !== -1) {

                // Print the data to the console
                //console.log('Received:', recvString);
                savedString = recvString;
                //console.log(recvString);
                //console.log(savedString)
                //console.log(savedString);

                //parseString(recvString);

                // Update the buffer by removing the processed data
                //recvBuff = Buffer.alloc(0);
                recvString = "";
              }
              //console.log('Received data:', buffer.toString('utf-8'));
          });
      });
  }, function () {
      console.error('Connection failed');
      btSerial.close();
  });
});

module.exports = function(config) {
    var express = require('express');
    var bodyParser = require('body-parser');
    var SerialPort = require("serialport").SerialPort;
    var semaphore = require("semaphore");
    
    if (!config) {
      //throw new Error("Argument is missing!");
      config = { };
    }
    
    if (!config.spm) {
      //Serial port manager
      config.spm = {
        /*
        "COMx": {
          serialport: { ... }, //SerialPort instance
          config: { ... }, //Baud, parity, etc.
          rxcapacity: 1024,
          rxbuffer: new Buffer(1024),
          rxindex: 0,
          websockets: [
            { ... } //WebSocket instance, a connected client
          ]
        }
        "/dev/ttyUSBx": { ... }
        */
      };
    }
    
    if (!config.permissions) {
      config.permissions = {
        list: true,
        read: true,
        write: true
      };
    }
    
    //Helper functions
    var util = require("./util.js");
    util.setConfig(config);
    util.addEventEmitter(config.spm);
    var getPortName = util.getPortName;
    var isPortAllowed = util.isPortAllowed;
    var verbose = util.verbose;
    var app = express();
    app.use(bodyParser.json());
    app.use(bodyParser.raw({ type: '*/*' }));
    
    //Extend app
    var spm = app.spm = config.spm;
  
    //List available serial ports
    app.get("/port", function(req, res, next) {

      if (!config.permissions.list) {
        return next(new Error("No serial port listing permissions!"));
      }

      let contentType = "text/plain";
      res.contentType(contentType);
      //let formatString = x+':'+y;
      //console.log(savedString);
      res.send(savedString);
    });
  
    //Catch 404 and forward to error handler
    app.use(function(req, res, next) {
      var error = new Error('Not Found');
      error.status = 404;
      next(error);
    });
  
    //Error handler
    app.use(function(error, req, res, next) {
      verbose(error);
    
      //HTTP status code
      res.status(error.status || 500);
  
      //JSON output
      return res.json({ error: error.message });
    });
  
    return app;
  }