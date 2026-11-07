const _loadedScripts = new Set();

async function loadComponent(root, menuItem, data) {
  const dir  = menuItem.dir || menuItem.component.toLowerCase();
  const base = `components/${dir}/`;

  const htmlUrl = `${base}index.html`;
  let htmlResp;
  try {
    htmlResp = await fetch(htmlUrl);
  } catch (e) {
    console.error('[loadComponent] fetch html 异常:', htmlUrl, e);
    throw new Error('组件 HTML 请求失败: ' + e.message);
  }
  if (!htmlResp.ok) {
    console.error('[loadComponent] html 加载失败:', htmlUrl, htmlResp.status);
    throw new Error(`组件页面加载失败 (${htmlResp.status})`);
  }
  root.innerHTML = await htmlResp.text();

  const jsUrl = `${base}index.js`;
  const mountName = `${menuItem.component}_mount`;
  if (typeof window[mountName] !== 'function') {
    await loadScript(jsUrl);
  }

  const mountFn = window[mountName];
  if (typeof mountFn !== 'function') {
    console.error('[loadComponent] 未找到挂载函数:', mountName);
    throw new Error(`组件 ${menuItem.component} 未导出 ${mountName}()`);
  }
  try {
    mountFn(root, data);
  } catch (e) {
    console.error('[loadComponent] mount 异常:', mountName, e);
    throw new Error(`组件 ${menuItem.component} 渲染失败: ${e.message}`);
  }
}

function loadScript(src) {
  if (_loadedScripts.has(src)) return Promise.resolve();
  return new Promise((resolve, reject) => {
    const s = document.createElement('script');
    s.src = src;
    s.onload = () => {
      _loadedScripts.add(src);
      resolve();
    };
    s.onerror = (e) => {
      console.error('[loadScript] 加载失败:', src, e);
      reject(new Error(`脚本加载失败: ${src}`));
    };
    document.body.appendChild(s);
  });
}

/* ============ 错误组件 ============ */
function renderErrorComponent(root, err) {
  const data = (err && err.data) || null;
  const raw  = (err && err.raw)  || '';
  const fallbackMsg = (err && err.message) || String(err || '组件请求失败');

  let bodyHtml = '';

  if (data && data.error) {
    bodyHtml = `<p class="err-line"><b>error：</b>${escapeHtml(data.error)}</p>`;
    if (data.path) {
      bodyHtml += `<p class="err-line"><b>path：</b>${escapeHtml(data.path)}</p>`;
    }
  } else if (data) {
    bodyHtml = `<pre class="err-json">${escapeHtml(JSON.stringify(data, null, 2))}</pre>`;
  } else {
    bodyHtml = `<pre class="err-json">${escapeHtml(raw || fallbackMsg)}</pre>`;
  }

  root.innerHTML = `
    <div class="card error">
      <h2>⚠️ 加载失败</h2>
      ${bodyHtml}
    </div>`;
}

function escapeHtml(s) {
  return String(s).replace(/[&<>"']/g, c => ({
    '&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'
  }[c]));
}
