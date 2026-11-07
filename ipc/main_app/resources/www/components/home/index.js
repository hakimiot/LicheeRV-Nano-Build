window.Home_mount = function (root, data) {
  const d = data || {};
  const devKey = root.querySelector('#home-device-key');
  const devVer = root.querySelector('#home-device-version');
  const devName  = root.querySelector('#home-device-name');
  
  const capWidth = root.querySelector('#home-capture-width');
  const capHeight = root.querySelector('#home-capture-height');
  const capFps  = root.querySelector('#home-capture-fps');
  
  const liveCon = root.querySelector('#home-live-connect');
  const liveSta = root.querySelector('#home-live-status');
  const liveUrl  = root.querySelector('#home-live-url');
  
  const replayCon = root.querySelector('#home-replay-connect');
  const replaySta = root.querySelector('#home-replay-status');
  const replayUrl  = root.querySelector('#home-replay-url');

  const netCon = root.querySelector('#home-network-connect');
  const netIp = root.querySelector('#home-network-ip');
  const netMac = root.querySelector('#home-network-mac');
  const netGateway = root.querySelector('#home-network-gateway');

  const memTotal = root.querySelector('#home-memory-total');
  const memFree = root.querySelector('#home-memory-free');
  const memUsed = root.querySelector('#home-memory-used');

  const diskTotal = root.querySelector('#home-disk-total');
  const diskFree = root.querySelector('#home-disk-free');
  const diskUsed = root.querySelector('#home-disk-used');

  const cpuLoad = root.querySelector('#home-cpu-load');
  const cpuTemp = root.querySelector('#home-cpu-temp');
  const cpuTime = root.querySelector('#home-cpu-time');

  devKey.textContent = d.system.info.key  || '-';
  devVer.textContent = d.system.info.version || '-';
  devName.textContent  = d.system.info.name || '-';
  
  capWidth.textContent = d.capture.width  || '-';
  capHeight.textContent = d.capture.height || '-';
  capFps.textContent  = d.capture.fps || '-';
  
  liveCon.textContent = fmtBool(d.rtmp.live.connect, '已连接', '未连接');
  liveSta.textContent = fmtBool(d.rtmp.live.stream, '推流中', '已停止');
  liveUrl.textContent  = d.rtmp.live.url || '-';
  
  replayCon.textContent = fmtBool(d.rtmp.replay.connect, '已连接', '未连接');
  replaySta.textContent = fmtBool(d.rtmp.replay.stream, '推流中', '已停止');
  replayUrl.textContent  = d.rtmp.replay.url || '-';

  // netCon.textContent = fmtBool(d.network.connect, '已连接', '未连接');
  // netIp.textContent = d.network.ip || '-';
  // netMac.textContent = d.network.mac || '-';
  // netGateway.textContent = d.network.gateway || '-';
  
  memTotal.textContent = d.system.memory.total || '-';
  memFree.textContent = d.system.memory.free || '-';
  memUsed.textContent = d.system.memory.used || '-';
  
  diskTotal.textContent = d.system.disk.total || '-';
  diskFree.textContent = d.system.disk.free || '-';
  diskUsed.textContent = d.system.disk.used || '-';
  
  cpuLoad.textContent = d.system.cpu.load || '-';
  cpuTemp.textContent = d.system.cpu.temp || '-';
  cpuTime.textContent = formatUptime(d.system.cpu.uptime) || '-';
};

function formatUptime(sec) {
  const n = Number(sec) || 0;
  const h = Math.floor(n / 3600), m = Math.floor(n % 3600 / 60), s = n % 60;
  return `${h}时${m}分${s}秒`;
}

function fmtBool(v, trueText = '是', falseText = '否') {
  if (v === true)  return trueText;
  if (v === false) return falseText;
  if (v === undefined || v === null || v === '') return '-';
  return String(v);
}
