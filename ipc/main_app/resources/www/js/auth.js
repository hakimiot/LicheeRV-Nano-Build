function showLoginPage() {
  document.getElementById('page-login').style.display = 'flex';
  document.getElementById('page-main').style.display  = 'none';
  const tip = document.getElementById('login-tip');
  if (tip) tip.textContent = '';
  document.getElementById('login-user').value = '';
  document.getElementById('login-pass').value = '';
}

function showMainPage() {
  document.getElementById('page-login').style.display = 'none';
  document.getElementById('page-main').style.display  = 'flex';
}

/* ============ 登录 ============ */
async function doLogin(username, password) {
  let resp;
  try {
    resp = await fetch('/api/user/login', {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify({ username, password }),
    });
  } catch (e) {
    console.error('[doLogin] 网络异常:', e);
    throw new Error('网络请求失败: ' + e.message);
  }

  const text = await resp.text();
  let data = null;
  try { data = JSON.parse(text); } catch (_) {}

  if (!data || typeof data !== 'object') {
    console.error('[doLogin] 响应非 JSON:', text);
    throw new Error(`登录失败 (HTTP ${resp.status})`);
  }

  if (data.code !== 200) {
    console.error('[doLogin] 登录失败:', data);
    throw new Error(data.error || `登录失败 (code ${data.code})`);
  }

  if (typeof data.token !== 'string' || !data.token) {
    console.error('[doLogin] 缺少 token:', data);
    throw new Error('登录响应缺少 token');
  }

  if (!setToken(data.token)) {
    console.error('[doLogin] token 保存失败');
    throw new Error('token 保存失败');
  }
  if (data.ip) localStorage.setItem(IP_KEY, data.ip);
  return data;
}

function tryAutoLogin() {
  return !!getToken();
}

function bindLoginEvents() {
  document.getElementById('login-form').addEventListener('submit', async (e) => {
    e.preventDefault();
    const tip = document.getElementById('login-tip');
    tip.textContent = '';
    const btn = document.getElementById('login-btn');
    btn.disabled = true;
    try {
      await doLogin(
        document.getElementById('login-user').value.trim(),
        document.getElementById('login-pass').value
      );
      await enterMain();
    } catch (err) {
      console.error('[login] 失败:', err);
      tip.textContent = err.message;
    } finally {
      btn.disabled = false;
    }
  });
}

async function enterMain() {
  showMainPage();

  try {
    renderMenu(MENU_CONFIG, onMenuClick);
  } catch (e) {
    console.error('[enterMain] renderMenu 异常:', e);
    return;
  }

  const first = findFirstLeaf(MENU_CONFIG);
  if (first) {
    try {
      selectMenu(first.id);
    } catch (e) {
      console.error('[enterMain] selectMenu 异常:', e);
    }
  } else {
    console.warn('[enterMain] 没有叶子菜单');
  }
}
