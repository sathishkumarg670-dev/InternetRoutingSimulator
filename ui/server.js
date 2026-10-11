
"use strict";

const http = require("node:http");
const net = require("node:net");
const fs = require("node:fs");
const path = require("node:path");

const { discoverDevices, getNetworkInfo } = require("./devices");

const WEB_HOST = "127.0.0.1";
const WEB_PORT = 3000;
const RECEIVER_PORT = 5000;
const MAX_BODY_SIZE = 4096;
const MAX_MESSAGE_LENGTH = 1000;

const HTML_FILE = path.join(__dirname, "public", "index.html");

function sendJson(res, status, data) {
  if (res.writableEnded || res.destroyed) return;

  res.writeHead(status, {
    "Content-Type": "application/json; charset=utf-8",
    "Cache-Control": "no-store",
    "X-Content-Type-Options": "nosniff"
  });

  res.end(JSON.stringify(data));
}

function isValidIPv4(ip) {
  if (typeof ip !== "string") return false;

  const parts = ip.split(".");
  if (parts.length !== 4) return false;

  return parts.every(part =>
    /^\d{1,3}$/.test(part) &&
    Number(part) >= 0 &&
    Number(part) <= 255
  );
}

function isUsableDestination(ip) {
  if (!isValidIPv4(ip)) return false;

  const octets = ip.split(".").map(Number);

  // Reject unspecified, broadcast, and multicast destinations.
  if (octets[0] === 0 || octets[0] >= 224) return false;
  if (octets.every(value => value === 255)) return false;

  return true;
}

function readJsonBody(req) {
  return new Promise((resolve, reject) => {
    let body = "";
    let tooLarge = false;

    req.on("data", chunk => {
      if (tooLarge) return;

      body += chunk.toString("utf8");

      if (Buffer.byteLength(body, "utf8") > MAX_BODY_SIZE) {
        tooLarge = true;
        body = "";
      }
    });

    req.on("end", () => {
      if (tooLarge) {
        reject(Object.assign(
          new Error("Request is too large."),
          { statusCode: 413 }
        ));
        return;
      }

      try {
        resolve(JSON.parse(body));
      } catch {
        reject(Object.assign(
          new Error("Invalid JSON request."),
          { statusCode: 400 }
        ));
      }
    });

    req.on("error", error => reject(error));
  });
}

function sendToDevice(host, message) {
  return new Promise((resolve, reject) => {
    const socket = net.createConnection({
      host,
      port: RECEIVER_PORT,
      family: 4
    });

    let response = "";
    let finished = false;

    function fail(error) {
      if (finished) return;

      finished = true;
      socket.destroy();

      reject(error instanceof Error
        ? error
        : new Error(String(error)));
    }

    socket.setTimeout(10000);

    socket.on("connect", () => {
      socket.write(message + "\n", error => {
        if (error) fail(error);
      });
    });

    socket.on("data", chunk => {
      if (finished) return;

      response += chunk.toString("utf8");

      // Prevent an unexpectedly large response from a receiver.
      if (response.length > 1024) {
        fail("Receiver returned an unexpectedly large response.");
        return;
      }

      const newlineIndex = response.indexOf("\n");
      if (newlineIndex === -1) return;

      const acknowledgement = response
        .slice(0, newlineIndex)
        .trim();

      if (acknowledgement !== "DELIVERED") {
        fail(
          "Unexpected receiver acknowledgement: " +
          (acknowledgement || "(empty)")
        );
        return;
      }

      finished = true;
      socket.end();

      resolve(`DELIVERED to ${host}:${RECEIVER_PORT}`);
    });

    socket.on("timeout", () => {
      fail(`Receiver at ${host}:${RECEIVER_PORT} timed out.`);
    });

    socket.on("error", error => {
      fail(
        `TCP connection to ${host}:${RECEIVER_PORT} failed: ` +
        error.message
      );
    });

    socket.on("close", () => {
      if (!finished) {
        fail("Receiver closed without acknowledging delivery.");
      }
    });
  });
}

async function handleDevices(res) {
  try {
    const devices = await discoverDevices();

    sendJson(res, 200, {
      devices: Array.isArray(devices) ? devices : [],
      network: getNetworkInfo()
    });
  } catch (error) {
    sendJson(res, 500, {
      error: `Device discovery failed: ${error.message}`
    });
  }
}

function handleNetwork(res) {
  try {
    sendJson(res, 200, getNetworkInfo());
  } catch (error) {
    sendJson(res, 500, {
      error: `Network information failed: ${error.message}`
    });
  }
}

function handleHome(res) {
  fs.readFile(HTML_FILE, (error, html) => {
    if (error) {
      sendJson(res, 500, {
        error: "Could not load ui/public/index.html."
      });
      return;
    }

    res.writeHead(200, {
      "Content-Type": "text/html; charset=utf-8",
      "Cache-Control": "no-store",
      "X-Content-Type-Options": "nosniff"
    });

    res.end(html);
  });
}

async function handleSend(req, res) {
  let payload;

  try {
    payload = await readJsonBody(req);
  } catch (error) {
    sendJson(res, error.statusCode || 400, {
      error: error.message || "Could not read request."
    });
    return;
  }

  /*
   * Accept both frontend formats:
   *
   * { "destination": "192.168.1.2", "message": "Hello" }
   *
   * and:
   *
   * { "ip": "192.168.1.2", "message": "Hello", "port": 5000 }
   *
   * The receiver port is deliberately fixed at 5000.
   */
  const destination = payload.destination ?? payload.ip;
  const message = payload.message;

  if (!isUsableDestination(destination)) {
    sendJson(res, 400, {
      delivered: false,
      error: "Choose a valid discovered device by IP address."
    });
    return;
  }

  if (typeof message !== "string" || !message.trim()) {
    sendJson(res, 400, {
      delivered: false,
      error: "Enter a message first."
    });
    return;
  }

  if (
    message.length > MAX_MESSAGE_LENGTH ||
    /[\r\n]/.test(message)
  ) {
    sendJson(res, 400, {
      delivered: false,
      error: "Use up to 1000 characters on a single line."
    });
    return;
  }

  try {
    // Only send to devices currently returned by Windows discovery.
    const discovered = await discoverDevices();
    const device = discovered.find(item => item.ip === destination);

    if (!device) {
      sendJson(res, 400, {
        delivered: false,
        destination,
        error:
          "That IP is not in the current discovered-device list. " +
          "Refresh devices and try again."
      });
      return;
    }

    const response = await sendToDevice(device.ip, message.trim());

    sendJson(res, 200, {
      success: true,
      delivered: true,
      destination: device.ip,
      name: device.name || "Unknown device",
      response
    });
  } catch (error) {
    sendJson(res, 502, {
      success: false,
      delivered: false,
      destination,
      error: error.message || "Message delivery failed."
    });
  }
}

const server = http.createServer(async (req, res) => {
  const pathname = (req.url || "").split("?")[0];

  try {
    if (req.method === "GET" && pathname === "/") {
      handleHome(res);
      return;
    }

    if (req.method === "GET" && pathname === "/api/network") {
      handleNetwork(res);
      return;
    }

    if (req.method === "GET" && pathname === "/api/devices") {
      await handleDevices(res);
      return;
    }

    if (req.method === "POST" && pathname === "/api/send") {
      await handleSend(req, res);
      return;
    }

    sendJson(res, 404, {
      error: "Not found."
    });
  } catch (error) {
    console.error("Request error:", error);

    sendJson(res, 500, {
      error: "An unexpected server error occurred."
    });
  }
});

server.on("error", error => {
  console.error(`NetPath server error: ${error.message}`);

  if (error.code === "EADDRINUSE") {
    console.error(
      `Port ${WEB_PORT} is already in use. ` +
      "Stop the existing NetPath server before restarting."
    );
  }

  process.exitCode = 1;
});

server.listen(WEB_PORT, WEB_HOST, () => {
  console.log("");
  console.log("====================================");
  console.log(" NetPath - Real Network Dashboard");
  console.log("====================================");
  console.log(`Dashboard: http://${WEB_HOST}:${WEB_PORT}`);
  console.log(`Receiver TCP port: ${RECEIVER_PORT}`);
  console.log("Network API: /api/network");
  console.log("Devices API: /api/devices");
  console.log("Message API: /api/send");
  console.log("Network monitoring: enabled in frontend");
  console.log("====================================");
  console.log("");
});
