const contentEl = () => document.getElementById('content');

async function onMenuClick(menuItem) {
  const box = contentEl();
  if (!box) {
    console.error('[onMenuClick] 找不到 #content');
    return;
  }
  box.innerHTML = `<div class="loading">加载中…</div>`;

  try {
    let data = null;
    if (menuItem.api) {
      data = await apiPost(menuItem.api, {});
    }
    await loadComponent(box, menuItem, data);
  } catch (err) {
    renderErrorComponent(box, err);   // 业务错误用卡片展示，不打日志
  }
}

async function boot() {
  try {
    bindLoginEvents();
  } catch (e) {
    console.error('[boot] bindLoginEvents 异常:', e);
    return;
  }

  try {
    if (tryAutoLogin()) {
      await enterMain();
    } else {
      showLoginPage();
    }
  } catch (e) {
    console.error('[boot] 启动异常:', e);
    showLoginPage();
  }
}

if (document.readyState === 'loading') {
  document.addEventListener('DOMContentLoaded', boot);
} else {
  boot();
}
