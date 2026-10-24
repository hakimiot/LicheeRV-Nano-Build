window.Settings_mount = function (root, data) {
  const d = data || {};
  const nameEl  = root.querySelector('#cfg-name');
  const intEl   = root.querySelector('#cfg-interval');
  const saveEl  = root.querySelector('#cfg-save');
  const resetEl = root.querySelector('#cfg-reset');

  if (!nameEl || !intEl || !saveEl || !resetEl) {
    console.error('[Settings_mount] 缺少 DOM 元素');
    return;
  }

  nameEl.value = d.name || '';
  intEl.value  = d.interval || 1000;

  saveEl.onclick = async () => {
    try {
      await apiPost('/api/settings/save', {
        name: nameEl.value.trim(),
        interval: Number(intEl.value)
      });
      alert('保存成功');
    } catch (e) {
      alert('保存失败：' + e.message);
    }
  };

  resetEl.onclick = async () => {
    if (!confirm('确认恢复出厂设置？')) return;
    try {
      await apiPost('/api/system/reset', {});
      alert('已恢复出厂设置');
    } catch (e) {
      alert('操作失败：' + e.message);
    }
  };
};
