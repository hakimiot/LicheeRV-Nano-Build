const MENU_CONFIG = [
  {
    id: 'home',
    title: '主页',
    icon: 'icons/menu/home.svg',
    component: 'Home',
    dir: 'home',
    api: '/api/app/config',
    children: []
  },
  {
    id: 'system',
    title: '系统',
    icon: 'icons/menu/system.svg',
    children: [
      {
        id: 'settings',
        title: '设置',
        icon: 'icons/menu/settings.svg',
        component: 'Settings',
        dir: 'settings',
        api: '/api/settings/info'
      }
    ]
  }
];
