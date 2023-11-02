project('ryzen', 'cpp')

cc = meson.get_compiler('cpp')
here = meson.current_source_dir()

platform = cc.find_library('platform', dirs : [ here/'lib' ])
device = cc.find_library('device', dirs : [ here/'lib' ])

ryzen = declare_dependency(
    include_directories : 'include',
    dependencies : [ platform, device ],
    compile_args : [ '/wd4201' ] # nonstandard extension used: nameless struct/union
)
