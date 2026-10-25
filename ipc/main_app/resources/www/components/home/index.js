window.Home_mount = function (root, data) {
  const d = data || {};
  const devModel = root.querySelector('#home-device-model');
  const devVer = root.querySelector('#home-device-version');
  const devUp  = root.querySelector('#home-device-uptime');
  
  const capWidth = root.querySelector('#home-capture-width');
  const capHeight = root.querySelector('#home-capture-height');
  const capFps  = root.querySelector('#home-capture-fps');
  
  const liveCon = root.querySelector('#home-live-connect');
  const liveSta = root.querySelector('#home-live-status');
  const liveUrl  = root.querySelector('#home-live-url');
  
  const replayCon = root.querySelector('#home-replay-connect');
  const replaySta = root.querySelector('#home-replay-status');
  const replayUrl  = root.querySelector('#home-replay-url');


  devModel.textContent = d.info.device  || '-';
  devVer.textContent = d.info.version || '-';
  devUp.textContent  = formatUptime(d.info.uptime || 0);
  
  capWidth.textContent = d.capture.width  || '-';
  capHeight.textContent = d.capture.height || '-';
  capFps.textContent  = d.capture.fps || '-';
  
  liveCon.textContent = fmtBool(d.rtmp.live.connect, '已连接', '未连接');
  liveSta.textContent = fmtBool(d.rtmp.live.stream, '推流中', '已停止');
  liveUrl.textContent  = d.rtmp.live.url || '-';
  
  replayCon.textContent = fmtBool(d.rtmp.replay.connect, '已连接', '未连接');
  replaySta.textContent = fmtBool(d.rtmp.replay.stream, '推流中', '已停止');
  replayUrl.textContent  = d.rtmp.replay.url || '-';
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
