project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static'
    ]
)

###
### deps
###

directx = subproject('directx').get_variable('directx')
stb = subproject('stb').get_variable('stb')

###
### engine
###

src = [
    'engine/src/engine.cpp',

    # os
    'engine/src/os/system.cpp',
    'engine/src/os/timer.cpp',

    # assets
    'engine/src/assets/assets.cpp',

    # render
    'engine/src/render/render.cpp',
    'engine/src/render/vendor.cpp',
]

deps = [
    # assets
    stb,

    # render
    directx
]

libengine = library('engine', src,
    include_directories : [ 'engine/include' ],
    dependencies : deps
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : [ 'engine/include' ],
    dependencies : deps
)

###
### editor
###

python3 = import('python').find_installation()

script_dir = meson.current_source_dir()/'data/scripts'

hlsl = generator(python3,
    output : [ '@BASENAME@.ps.cso', '@BASENAME@.vs.cso' ],
    arguments : [ script_dir/'compile-hlsl.py', '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'vs,ps', '6_0' ]
)

copy = generator(python3,
    output : '@PLAINNAME@',
    arguments : [ script_dir/'copy-asset.py', '@INPUT@', '@OUTPUT@' ]
)

src = [
    'editor/src/main.cpp',

    # render
    'editor/src/editor/render/render.cpp',
    'editor/src/editor/render/graph.cpp',

    # shaders
    hlsl.process('data/shaders/quad.hlsl'),
    hlsl.process('data/shaders/blit.hlsl'),

    # assets
    copy.process('data/images/uv-coords.png')
]

executable('editor', src,
    dependencies : engine,
    include_directories : [ 'editor/include' ]
)

###
### client
###

executable('client', 'client/src/main.cpp',
    dependencies : engine,
    include_directories : [ 'client/include' ]
)
