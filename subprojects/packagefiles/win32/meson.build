project('win32', 'c')

cc = meson.get_compiler('c')

dbghelp_dep = declare_dependency(dependencies : cc.find_library('dbghelp'))
netapi_dep = declare_dependency(dependencies : cc.find_library('netapi32'))
xinput_dep = declare_dependency(dependencies : cc.find_library('xinput'))

meson.override_dependency('win32_dbghlp', dbghelp_dep)
meson.override_dependency('win32_netapi', netapi_dep)
meson.override_dependency('win32_xinput', xinput_dep)
