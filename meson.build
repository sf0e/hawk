project('hawk', 'c',
  version: '0.1.0',
  default_options: ['c_std=c11', 'warning_level=1'],
)

gtk4_dep = dependency('gtk4')
webkit_dep = dependency('webkitgtk-6.0')

executable('hawk',
  sources: [
    'src/main.c',
    'src/browser.c',
    'src/backend.c',
    'src/settings.c',
  ],
  dependencies: [gtk4_dep, webkit_dep],
  install: true,
)

install_data('scripts/hawk-searchd',
  install_dir: get_option('libexecdir') / 'hawk',
  install_mode: 'rwxr-xr-x',
)

icon_sizes = ['16', '24', '32', '48', '64', '128', '256']
foreach size : icon_sizes
  install_data('data/icons/hicolor/' + size + '/apps/org.hawk.Hawk.png',
    install_dir: get_option('datadir') / 'icons' / 'hicolor' / size / 'apps',
  )
endforeach
install_data('data/icons/hicolor/scalable/apps/org.hawk.Hawk.svg',
  install_dir: get_option('datadir') / 'icons' / 'hicolor' / 'scalable' / 'apps',
)
install_data('data/org.hawk.Hawk.desktop',
  install_dir: get_option('datadir') / 'applications',
)