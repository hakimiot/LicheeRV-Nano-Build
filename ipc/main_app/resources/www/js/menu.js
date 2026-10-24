let currentMenuId = null;

function renderMenu(menus, onLeaf) {
  const root = document.getElementById('menu-root');
  if (!root) {
    console.error('[renderMenu] 找不到 #menu-root');
    return;
  }
  root.innerHTML = '';
  menus.forEach(m => {
    try {
      root.appendChild(buildMenuItem(m, onLeaf));
    } catch (e) {
      console.error('[renderMenu] 构建菜单项失败:', m, e);
    }
  });
}

function buildMenuItem(item, onLeaf) {
  const wrap = document.createElement('div');
  wrap.className = 'menu-item';

  const header = document.createElement('div');
  header.className = 'menu-title';
  header.dataset.id = item.id;

  const icon = item.icon
    ? `<img class="menu-icon" src="${item.icon}" alt="" onerror="console.warn('[menu] 图标加载失败:', this.src)">`
    : `<span class="menu-icon"></span>`;

  header.innerHTML = `
    ${icon}
    <span class="text">${item.title}</span>
    ${(item.children && item.children.length) ? '<span class="arrow">▶</span>' : ''}
  `;
  wrap.appendChild(header);

  if (item.children && item.children.length) {
    const sub = document.createElement('div');
    sub.className = 'menu-sub';
    item.children.forEach(c => sub.appendChild(buildMenuItem(c, onLeaf)));
    wrap.appendChild(sub);

    header.addEventListener('click', () => {
      const open = wrap.classList.toggle('open');
      sub.style.display = open ? 'block' : 'none';
    });
  } else {
    header.addEventListener('click', () => {
      if (currentMenuId === item.id) return;
      currentMenuId = item.id;
      highlight(item.id);
      try {
        onLeaf(item);
      } catch (e) {
        console.error('[menu] onLeaf 异常:', e);
      }
    });
  }
  return wrap;
}

function highlight(id) {
  document.querySelectorAll('.menu-title.active')
    .forEach(el => el.classList.remove('active'));
  const el = document.querySelector(`.menu-title[data-id="${id}"]`);
  if (el) el.classList.add('active');
  else console.error('[highlight] 找不到菜单:', id);
}

function selectMenu(id) {
  const item = findMenuById(MENU_CONFIG, id);
  if (!item) {
    console.error('[selectMenu] 找不到菜单:', id);
    return;
  }
  expandAncestors(MENU_CONFIG, id);
  currentMenuId = id;
  highlight(id);
  try {
    onMenuClick(item);
  } catch (e) {
    console.error('[selectMenu] onMenuClick 异常:', e);
  }
}

function findMenuById(menus, id) {
  for (const m of menus) {
    if (m.id === id) return m;
    if (m.children) {
      const r = findMenuById(m.children, id);
      if (r) return r;
    }
  }
  return null;
}

function expandAncestors(menus, id, path = []) {
  for (const m of menus) {
    const cur = [...path, m];
    if (m.id === id) {
      cur.forEach(node => {
        if (node.children && node.children.length) {
          const title = document.querySelector(`.menu-title[data-id="${node.id}"]`);
          if (title) {
            const wrap = title.parentElement;
            wrap.classList.add('open');
            const sub = wrap.querySelector('.menu-sub');
            if (sub) sub.style.display = 'block';
          }
        }
      });
      return true;
    }
    if (m.children && expandAncestors(m.children, id, cur)) return true;
  }
  return false;
}

function findFirstLeaf(menus) {
  for (const m of menus) {
    if (m.children && m.children.length) {
      const r = findFirstLeaf(m.children);
      if (r) return r;
    } else {
      return m;
    }
  }
  return null;
}
