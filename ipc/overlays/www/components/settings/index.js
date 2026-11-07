window.Settings_mount = function (root) {
  const rtmpUrlEl  = root.querySelector('#cfg-rtmp-url');
  const rtmpSaveEl = root.querySelector('#cfg-rtmp-save');

  const capWidthEl  = root.querySelector('#cfg-cap-width');
  const capHeightEl = root.querySelector('#cfg-cap-height');
  const capFpsEl    = root.querySelector('#cfg-cap-fps');
  const capSaveEl   = root.querySelector('#cfg-cap-save');

  const rebootEl = root.querySelector('#cfg-reboot');

  if (!rtmpUrlEl || !rtmpSaveEl || !capWidthEl || !capHeightEl || !capFpsEl || !capSaveEl || !rebootEl) {
    console.error('[Settings_mount] 缺少 DOM 元素');
    return;
  }

  // 加载 RTMP 服务器地址（从 app/config 取直播推流地址，去掉末尾的设备 key 后缀）
  apiPost('/api/app/config', {}).then(data => {
    const url = (data && data.rtmp && data.rtmp.live && data.rtmp.live.url) ? String(data.rtmp.live.url) : '';
    rtmpUrlEl.value = stripDeviceKeySuffix(url);
  }).catch(e => {
    console.warn('[Settings] 获取 RTMP 地址失败:', e.message);
  });

  // 加载采集参数
  apiPost('/api/capture/get', {}).then(data => {
    capWidthEl.value  = data.width  || '';
    capHeightEl.value = data.height || '';
    capFpsEl.value    = data.fps    || '';
  }).catch(e => {
    console.warn('[Settings] 获取采集参数失败:', e.message);
  });

  // 保存 RTMP 地址
  rtmpSaveEl.onclick = async () => {
    const url = rtmpUrlEl.value.trim();
    if (!url) {
      alert('请输入 RTMP 服务器地址');
      return;
    }
    try {
      await apiPost('/api/stream/seturl', { url: url });
      alert('保存成功');
    } catch (e) {
      alert('保存失败：' + e.message);
    }
  };

  // 保存采集参数
  capSaveEl.onclick = async () => {
    const width  = Number(capWidthEl.value);
    const height = Number(capHeightEl.value);
    const fps    = Number(capFpsEl.value);
    if (!width || !height || !fps) {
      alert('请填写完整的采集参数（宽、高、帧率）');
      return;
    }
    try {
      await apiPost('/api/capture/set', { width, height, fps });
      alert('保存成功');
    } catch (e) {
      alert('保存失败：' + e.message);
    }
  };

  // 重启设备
  rebootEl.onclick = async () => {
    if (!confirm('确认重启设备？')) return;
    try {
      await apiPost('/api/system/reset', {});
      alert('设备正在重启…');
    } catch (e) {
      alert('操作失败：' + e.message);
    }
  };
};

/**
 * 去除 RTMP 地址中的设备 key 后缀。
 * 实际推流地址形如 rtmp://host:port/live/stream/<key>
 * 或 rtmp://host:port/replay/stream/<key>，
 * 配置中只保存基础地址 rtmp://host:port。
 * 若传入的地址包含路径后缀，则将其剥离。
 */
function stripDeviceKeySuffix(url) {
  if (!url) return '';
  // 去掉末尾的 /live/stream/xxx 或 /replay/stream/xxx
  let result = url.replace(/\/(live|replay)\/stream\/[^\/]*\/*$/, '');
  // 去掉末尾多余的斜杠
  result = result.replace(/\/+$/, '');
  return result;
}
