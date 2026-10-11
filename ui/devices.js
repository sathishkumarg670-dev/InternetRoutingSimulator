const os = require("node:os");
const { execFile } = require("node:child_process");
const { promisify } = require("node:util");

const execFileAsync = promisify(execFile);

function ipv4ToInt(ip) {
  return ip.split(".").reduce((value, part) => ((value << 8) | Number(part)) >>> 0, 0);
}

function intToIpv4(value) {
  return [24, 16, 8, 0].map(shift => (value >>> shift) & 255).join(".");
}

function getActiveIpv4Interfaces() {
  const results = [];
  const interfaces = os.networkInterfaces();

  for (const [name, entries] of Object.entries(interfaces)) {
    for (const entry of entries || []) {
      if (entry.family !== "IPv4" || entry.internal) continue;
      if (entry.address.startsWith("169.254.")) continue;

      const prefixLength = Number(entry.cidr?.split("/")[1]);
      if (!Number.isInteger(prefixLength) || prefixLength < 1 || prefixLength > 32) continue;

      const mask = prefixLength === 32 ? 0xffffffff : (0xffffffff << (32 - prefixLength)) >>> 0;
      const ipInt = ipv4ToInt(entry.address);
      const networkInt = (ipInt & mask) >>> 0;

      results.push({
        interface: name,
        ip: entry.address,
        cidr: entry.cidr,
        prefixLength,
        network: `${intToIpv4(networkInt)}/${prefixLength}`,
        networkInt,
        mask
      });
    }
  }

  return results;
}

function isInActiveSubnet(ip, interfaces) {
  if (typeof ip !== "string" || !/^(?:\d{1,3}\.){3}\d{1,3}$/.test(ip)) return false;
  const octets = ip.split(".").map(Number);
  if (octets.some(n => n < 0 || n > 255)) return false;
  if (ip.startsWith("127.") || ip.startsWith("169.254.") || ip.startsWith("224.") || ip === "255.255.255.255") return false;

  const target = ipv4ToInt(ip);
  return interfaces.some(item => ((target & item.mask) >>> 0) === item.networkInt);
}

async function getWindowsNeighbors() {
  const script = [
    "$ErrorActionPreference = 'Stop'",
    "$items = Get-NetNeighbor -AddressFamily IPv4 | Where-Object { $_.IPAddress -and $_.LinkLayerAddress -and $_.State -notin @('Unreachable','Incomplete') } | Select-Object IPAddress,LinkLayerAddress,State,InterfaceAlias",
    "$items | ConvertTo-Json -Compress"
  ].join("; ");

  const { stdout } = await execFileAsync(
    "powershell.exe",
    ["-NoProfile", "-ExecutionPolicy", "Bypass", "-Command", script],
    { windowsHide: true, timeout: 10000, maxBuffer: 1024 * 1024 }
  );

  const raw = stdout.trim();
  if (!raw) return [];
  const parsed = JSON.parse(raw);
  return Array.isArray(parsed) ? parsed : [parsed];
}

async function resolveName(ip) {
  // Reverse DNS is best-effort only. Discovery still works if no name exists.
  try {
    const { stdout } = await execFileAsync(
      "powershell.exe",
      ["-NoProfile", "-Command", `[System.Net.Dns]::GetHostEntry('${ip}').HostName`],
      { windowsHide: true, timeout: 1800, maxBuffer: 16 * 1024 }
    );
    const name = stdout.trim();
    return name && name !== ip ? name.replace(/\.$/, "") : "Unknown device";
  } catch {
    return "Unknown device";
  }
}

async function discoverDevices() {
  const activeInterfaces = getActiveIpv4Interfaces();
  if (!activeInterfaces.length) return [];

  const neighbors = await getWindowsNeighbors();
  const filtered = neighbors.filter(item => isInActiveSubnet(item.IPAddress, activeInterfaces));

  const deduped = new Map();
  for (const item of filtered) {
    const ip = item.IPAddress;
    if (!deduped.has(ip)) {
      deduped.set(ip, {
        ip,
        mac: item.LinkLayerAddress,
        state: item.State,
        interface: item.InterfaceAlias || "Unknown interface",
        name: "Unknown device"
      });
    }
  }

  const devices = [...deduped.values()];
  await Promise.all(devices.map(async device => {
    device.name = await resolveName(device.ip);
  }));

  return devices.sort((a, b) => {
    const aa = ipv4ToInt(a.ip);
    const bb = ipv4ToInt(b.ip);
    return aa - bb;
  });
}

function getNetworkInfo() {
  const interfaces = getActiveIpv4Interfaces();
  return {
    interfaces: interfaces.map(({ interface: name, ip, cidr, network }) => ({
      name, ip, cidr, network
    })),
    fingerprint: interfaces
      .map(item => `${item.interface}|${item.ip}|${item.cidr}`)
      .sort()
      .join(";")
  };
}

module.exports = { discoverDevices, getNetworkInfo };
