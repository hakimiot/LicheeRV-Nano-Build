/* =========================================================
 *  token 持久化
 * ========================================================= */
const TOKEN_KEY = 'cfg_token';
const IP_KEY    = 'cfg_ip';

const TOKEN_FAIL_MSG = 'token verift failed';

function getToken() {
  try {
    const t = localStorage.getItem(TOKEN_KEY);
    if (!t || t === 'undefined' || t === 'null') return '';
    return t;
  } catch (e) {
    console.error('[getToken] 读取失败:', e);
    return '';
  }
}

function setToken(token) {
  try {
    if (typeof token !== 'string' || !token || token === 'undefined') {
      console.warn('[setToken] 非法 token，拒绝保存:', token);
      return false;
    }
    localStorage.setItem(TOKEN_KEY, token);
    return true;
  } catch (e) {
    console.error('[setToken] 保存失败:', e);
    return false;
  }
}

function clearToken() {
  try {
    localStorage.removeItem(TOKEN_KEY);
    localStorage.removeItem(IP_KEY);
  } catch (e) {
    console.error('[clearToken] 清除失败:', e);
  }
}

function isTokenFailed(code, errorMsg) {
  return code === 500 && errorMsg === TOKEN_FAIL_MSG;
}

/* =========================================================
 *  统一 POST
 * ========================================================= */
async function apiPost(url, body = {}) {
  const payload = { ...body, token: getToken() };

  let resp;
  try {
    resp = await fetch(url, {
      method: 'POST',
      headers: { 'Content-Type': 'application/json' },
      body: JSON.stringify(payload),
    });
  } catch (e) {
    console.error('[apiPost] fetch 异常:', url, e);
    const err = new Error('网络请求失败: ' + e.message);
    err.data = null; err.raw = ''; err.code = 0;
    throw err;
  }

  const text = await resp.text();
  let data = null;
  try { data = JSON.parse(text); } catch (_) { /* 非 JSON */ }

  // 非 JSON 或空 body：按 HTTP 状态判断
  if (!data || typeof data !== 'object') {
    if (!resp.ok) {
      const err = new Error(text || `HTTP ${resp.status}`);
      err.data = null; err.raw = text; err.code = resp.status;
      throw err;
    }
    return data;
  }

  const code     = data.code;
  const errorMsg = typeof data.error === 'string' ? data.error : '';

  if (code === 200) {
    return data;
  }

  if (isTokenFailed(code, errorMsg)) {
    onAuthFailed();          // 正常流程，不打日志
    throw new Error('登录已失效，请重新登录');
  }

  const err = new Error(errorMsg || `code ${code}`);
  err.data = data;
  err.raw  = text;
  err.code = code;
  throw err;
}

function onAuthFailed() {
  clearToken();
  showLoginPage();
}
