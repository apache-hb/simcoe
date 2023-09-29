project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static'
    ]
)

src = [
    'engine/src/engine.cpp',

    # os
    'engine/src/os/system.cpp',

    # render
    'engine/src/render/render.cpp'
]

links = [ 'd3d12.lib', 'dxgi.lib' ]

libengine = library('engine', src,
    include_directories : [ 'engine/include' ],
    link_args : links
)

engine = declare_dependency(
    link_with : libengine,
    link_args : links,
    include_directories : [ 'engine/include' ]
)

executable('editor', 'editor/src/main.cpp',
    dependencies : engine,
    include_directories : [ 'editor/include' ]
)

executable('client', 'client/src/main.cpp',
    dependencies : engine,
    include_directories : [ 'client/include' ]
)
