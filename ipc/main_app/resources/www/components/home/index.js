window.Home_mount = function (root, data) {
  const d = data || {};
  const dev = root.querySelector('#home-device');
  const ver = root.querySelector('#home-version');
  const up  = root.querySelector('#home-uptime');
  if (!dev || !ver || !up) {
    console.error('[Home_mount] 缺少 DOM 元素');
    return;
  }
  dev.textContent = d.device  || '-';
  ver.textContent = d.version || '-';
  up.textContent  = formatUptime(d.uptime || 0);
};

function formatUptime(sec) {
  const n = Number(sec) || 0;
  const h = Math.floor(n / 3600), m = Math.floor(n % 3600 / 60), s = n % 60;
  return `${h}时${m}分${s}秒`;
}
