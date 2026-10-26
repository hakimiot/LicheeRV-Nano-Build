window.Wlan_mount = function (root, data) {
  const listEl    = root.querySelector('#wlan-list');
  const refreshEl = root.querySelector('#wlan-refresh');
  if (!listEl) {
    console.error('[Wlan_mount] 缺少 #wlan-list');
    return;
  }

  const menuItem = (typeof MENU_CONFIG !== 'undefined')
    ? MENU_CONFIG.find(m => m.dir === 'wlan' || m.component === 'Wlan')
    : null;
  const scanApi = (menuItem && menuItem.api) ? menuItem.api : '/api/network/wifilist';

  renderList(listEl, data);

  if (refreshEl) {
    refreshEl.addEventListener('click', async () => {
      if (refreshEl.disabled) return;
      refreshEl.disabled = true;
      refreshEl.classList.add('loading');
      refreshEl.querySelector('span').textContent = '刷新中';

      try {
        const r = await apiPost(scanApi, {});
        renderList(listEl, r);
      } catch (e) {
        listEl.innerHTML = `<div class="wlan-empty">刷新失败：${escapeHtml(e.message)}</div>`;
      } finally {
        refreshEl.disabled = false;
        refreshEl.classList.remove('loading');
        refreshEl.querySelector('span').textContent = '刷新';
      }
    });
  }
};

/* ---------------- 渲染列表 ---------------- */
function renderList(listEl, data) {
  const list = Array.isArray(data && data.array) ? data.array : [];
  if (list.length === 0) {
    listEl.innerHTML = `<div class="wlan-empty">未搜索到 WiFi 网络</div>`;
    return;
  }

  const sorted = [...list].sort((a, b) => {
    if (a.isconnect !== b.isconnect) return a.isconnect ? -1 : 1;
    return (b.signal || -999) - (a.signal || -999);
  });

  listEl.innerHTML = sorted.map(item => {
    const ssid    = escapeHtml(item.ssid || '');
    const signal  = Number(item.signal) || -999;
    const connect = item.isconnect === true;
    const open    = item.isopen === true;

    const btn = connect
      ? `<button class="wlan-btn connected" disabled>已连接</button>`
      : `<button class="wlan-btn connect" data-ssid="${ssid}">连接</button>`;

    return `
      <div class="wlan-item${connect ? ' is-connected' : ''}">
        <img class="wlan-icon" src="${wifiIcon(signal, open)}" alt="">
        <span class="wlan-ssid" title="${ssid}">${ssid}</span>
        ${open ? '<span class="wlan-tag open">开放</span>' : ''}
        ${btn}
      </div>
    `;
  }).join('');

  // 绑定连接按钮
  listEl.querySelectorAll('.wlan-btn.connect').forEach(btn => {
    btn.addEventListener('click', async () => {
      const ssid = btn.dataset.ssid;
      btn.disabled = true;
      btn.textContent = '连接中…';
      try {
        const r = await apiPost('/api/wlan/connect', { ssid });
        if (r && r.code === 200) {
          btn.textContent = '已连接';
          btn.classList.remove('connect');
          btn.classList.add('connected');
          btn.closest('.wlan-item').classList.add('is-connected');
        } else {
          btn.textContent = '连接';
          btn.disabled = false;
          alert('连接失败：' + (r && r.error ? r.error : '未知错误'));
        }
      } catch (e) {
        btn.textContent = '连接';
        btn.disabled = false;
        alert('连接失败：' + e.message);
      }
    });
  });
}

/* ---------------- 图标映射 ---------------- */
function wifiIcon(dbm, isOpen) {
  let level;
  if (dbm >= -50)      level = 5;
  else if (dbm >= -60) level = 4;
  else if (dbm >= -70) level = 3;
  else if (dbm >= -80) level = 2;
  else                 level = 1;

  const suffix = isOpen ? '' : '-lock';
  return `icons/wlan/wifi-${level}${suffix}.svg`;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
  }[c]));
}
