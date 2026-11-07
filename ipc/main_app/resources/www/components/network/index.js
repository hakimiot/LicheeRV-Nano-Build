window.Network_mount = function (root, data) {
  const d = data || {};
  const list = Array.isArray(d.list) ? d.list : [];
  const container = root.querySelector('#iface-list');

  if (!container) {
    console.error('[Network_mount] 缺少 #iface-list');
    return;
  }

  if (list.length === 0) {
    container.innerHTML = '<div class="empty">未检测到网卡</div>';
    return;
  }

  container.innerHTML = list.map(renderIface).join('');
};

function renderIface(info) {
  const name = info.name || '-';
  const state = (info.state || '').toUpperCase();
  const stateClass = state === 'UP' ? 'up' : (state === 'DOWN' ? 'down' : 'unknown');

  const flags = Array.isArray(info.flags) ? info.flags : [];
  const flagsHtml = flags.map(f => `<span>${escapeHtml(f)}</span>`).join('');

  const ipv4List = Array.isArray(info.ipv4) ? info.ipv4 : [];
  const ipv6List = Array.isArray(info.ipv6) ? info.ipv6 : [];

  const ipv4Html = ipv4List.map(ip => {
    const addr = `${ip.address || '-'}/${ip.prefixLength || 0}`;
    const brd = ip.broadcast ? ` (广播 ${escapeHtml(ip.broadcast)})` : '';
    return `<div class="ip-item"><span class="ip-tag v4">IPv4</span>${escapeHtml(addr)}${brd}</div>`;
  }).join('');

  const ipv6Html = ipv6List.map(ip => {
    const addr = `${ip.address || '-'}/${ip.prefixLength || 0}`;
    const scope = ip.scope ? `<span class="ip-tag ${ip.scope}">${escapeHtml(ip.scope)}</span>` : '';
    const dynamic = ip.isDynamic ? '<span class="ip-tag dynamic">dynamic</span>' : '';
    return `<div class="ip-item"><span class="ip-tag v6">IPv6</span>${escapeHtml(addr)} ${scope} ${dynamic}</div>`;
  }).join('');

  const ipListHtml = (ipv4Html || ipv6Html)
    ? `<div class="ip-list">${ipv4Html}${ipv6Html}</div>`
    : '';

  return `
    <div class="iface-card">
      <div class="iface-header">
        <span class="iface-name">${escapeHtml(name)}</span>
        <span class="iface-state ${stateClass}">${escapeHtml(state || '-')}</span>
        <div class="iface-flags">${flagsHtml}</div>
      </div>
      <div class="iface-body">
        <p><b>接口序号：</b>${info.index ?? '-'}</p>
        <p><b>MTU：</b>${info.mtu ?? '-'}</p>
        <p><b>MAC 地址：</b>${escapeHtml(info.macAddress || '-')}</p>
        <p><b>广播 MAC：</b>${escapeHtml(info.broadcastMac || '-')}</p>
      </div>
      ${ipListHtml}
    </div>
  `;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
  }[c]));
}
