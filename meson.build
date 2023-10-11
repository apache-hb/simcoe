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
xinput = subproject('xinput').get_variable('xinput')
gameinput = subproject('gameinput').get_variable('gameinput')
stb = subproject('stb').get_variable('stb')

###
### engine
###

src = [
    'engine/src/engine.cpp',

    # os
    'engine/src/os/system.cpp',
    'engine/src/os/timer.cpp',

    # input
    'engine/src/input/input.cpp',
    'engine/src/input/win32-device.cpp',
    'engine/src/input/xinput-device.cpp',
    'engine/src/input/gameinput-device.cpp',

    # memory
    'engine/src/memory/bitmap.cpp',

    # assets
    'engine/src/assets/assets.cpp',

    # rhi
    'engine/src/rhi/render.cpp',
    'engine/src/rhi/vendor.cpp',

    # render graph
    'engine/src/render/render.cpp',
    'engine/src/render/graph.cpp',
    'engine/src/render/assets.cpp',

    # vendor
    'vendor/src/microsoft/gdk.cpp'
]

inc = [
    'engine/include',
    'vendor/include'
]

deps = [
    # input
    xinput, gameinput,

    # assets
    stb,

    # render
    directx
]

libengine = library('engine', src,
    include_directories : inc,
    dependencies : deps
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : inc,
    dependencies : deps
)

###
### editor
###

moodycamel = subproject('atomic-queue').get_variable('queue')
tinyobj = subproject('tinyobj').get_variable('tinyobj')
fastgltf = subproject('fastgltf').get_variable('fastgltf')

python3 = find_program('python3')

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

    # dear imgui
    'editor/src/imgui/imgui.cpp',
    'editor/src/imgui/imgui_draw.cpp',
    'editor/src/imgui/imgui_widgets.cpp',
    'editor/src/imgui/imgui_demo.cpp',
    'editor/src/imgui/imgui_tables.cpp',

    # dear imgui backends
    'editor/src/imgui/backends/imgui_impl_win32.cpp',
    'editor/src/imgui/backends/imgui_impl_dx12.cpp',

    # render graph objects
    'editor/src/editor/graph/assets.cpp',
    'editor/src/editor/graph/post.cpp',
    'editor/src/editor/graph/scene.cpp',
    'editor/src/editor/graph/gui.cpp',
    'editor/src/editor/graph/game.cpp',

    # game objects
    'editor/src/editor/game/obj.cpp',
    'editor/src/editor/game/gltf.cpp',

    # shaders
    hlsl.process('data/shaders/quad.hlsl'),
    hlsl.process('data/shaders/blit.hlsl'),
    hlsl.process('data/shaders/object.hlsl'),

    # assets
    copy.process('data/images/uv-coords.png'),
    copy.process('data/images/player.png'),

    # libraries
    copy.process('data/libs/warp/d3d10warp.dll'),
    copy.process('data/libs/warp/d3d10warp.pdb'),

    # config
    copy.process('data/config/MicrosoftGame.Config')
]

deps = [
    # general
    engine,

    # render
    tinyobj, fastgltf,

    # worker
    moodycamel,
]

executable('editor', src,
    dependencies : deps,
    include_directories : [ 'editor/include' ]
)

###
### client
###

src = [
    'client/src/main.cpp'
]

executable('client', src,
    dependencies : engine,
    include_directories : [ 'client/include' ]
)
