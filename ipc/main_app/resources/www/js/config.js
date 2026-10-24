const MENU_CONFIG = [
  {
    id: 'home',
    title: '主页',
    icon: 'icons/home.svg',
    component: 'Home',
    dir: 'home',
    api: '/api/app/config',
    children: []
  },
  {
    id: 'system',
    title: '系统',
    icon: 'icons/system.svg',
    children: [
      {
        id: 'settings',
        title: '设置',
        icon: 'icons/settings.svg',
        component: 'Settings',
        dir: 'settings',
        api: '/api/settings/info'
      }
    ]
  }
];
