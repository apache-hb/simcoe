project('simcoe', 'cpp',
    default_options : [
        'cpp_std=c++20',
        'default_library=static',
        'cpp_eh=default',
        'cpp_rtti=false',
        'b_vscrt=static_from_buildtype',

        'warning_level=3'
    ]
)

###
### config
###

buildtype = get_option('buildtype')
is_release = buildtype.contains('release')
is_debug = buildtype.contains('debug')

# if we're building in release mode, we dont want intrusive debugging
debug_threads = get_option('debug_threads').disable_auto_if(is_release)
debug_render = get_option('debug_d3d12').disable_auto_if(is_release)
debug_audio = get_option('debug_audio').disable_auto_if(is_release)

###
### services
###

tracy = dependency('tracy',
    fallback : [ 'tracy', 'tracy_dep' ],
    required : false
)

gameruntime = dependency('gameruntime',
    fallback : [ 'gameruntime', 'gameruntime_dep' ],
    required : false
)

ryzenmonitor = dependency('ryzenmonitor',
    fallback : [ 'ryzenmonitor', 'ryzenmonitor_dep' ],
    required : false
)

# TODO: some of these should be disabled in release
opt_tracy = get_option('tracy_enable') and tracy.found()
opt_gameruntime = get_option('gameruntime').enable_auto_if(gameruntime.found())
opt_ryzenmonitor = get_option('ryzenmonitor').enable_auto_if(ryzenmonitor.found())

# profiling
profile_startup = get_option('profile_startup').enable_auto_if(opt_tracy)

# if we're building in release mode, we want a gui app
gui_app = get_option('gui_app').enable_auto_if(is_release)

# if we're building for release, we want a windows store bundle
msstore_package = get_option('package_msstore')
steam_package = get_option('package_steam')

data_dir = meson.current_source_dir()/'data'
script_dir = data_dir/'scripts'

###
### deps
###

# argo = subproject('argo').get_variable('argo') TODO: config

# windows deps
dbghelp = dependency('win32_dbghlp')
xinput = dependency('win32_xinput')

# rendering stuff
directx = subproject('directx').get_variable('directx')

# config
tomlpp = dependency('tomlplusplus')
fmt = dependency('fmt')

# asset loading
stb = subproject('stb').get_variable('stb')
zlib = dependency('zlib')
libpng = dependency('libpng')
vorbis = dependency('vorbis')
vorbisfile = dependency('vorbisfile')

# font loading
harfbuzz = dependency('harfbuzz')
freetype2 = dependency('freetype2')

# threading
threads = dependency('threads')
moodycamel = dependency('concurrentqueue')

cdata = configuration_data()

# debug config
cdata.set10('SM_DEBUG', is_debug)
cdata.set10('SM_DEBUG_THREADS', debug_threads.allowed())
cdata.set10('SM_DEBUG_RENDER', debug_render.allowed())
cdata.set10('SM_DEBUG_AUDIO', debug_audio.allowed())

# profiling config
cdata.set10('SM_PROFILE_STARTUP', profile_startup.allowed())

# service config
cdata.set10('SM_SERVICE_DEBUG', is_debug)
cdata.set10('SM_SERVICE_LOGGING', true)

cdata.set10('SM_SERVICE_TRACY', opt_tracy)
cdata.set10('SM_SERVICE_RYZENMONITOR', opt_ryzenmonitor.allowed())
cdata.set10('SM_SERVICE_GAMERUNTIME', opt_gameruntime.allowed())

cfg_header = configure_file(output : 'sm-config.h', configuration : cdata)

###
### engine
###

args = [
    '/wd4324', # disable warning C4324: structure was padded due to alignment specifier
               # we want this because of UNIFORM_BUFFER
    '-D_HAS_EXCEPTIONS=0' # we dont want the stl to use exceptions, easier to debug that way
]

src = [
    # config
    cfg_header,

    # core
    'engine/src/core/panic.cpp',
    'engine/src/core/utf8.cpp',
    'engine/src/core/bitmap.cpp',
    'engine/src/core/strings.cpp',
    'engine/src/core/units.cpp',
    'engine/src/core/error.cpp',

    # config
    'engine/src/config/service.cpp',
    'engine/src/config/system.cpp',
    'engine/src/config/toml.cpp',

    # service
    'engine/src/service/service.cpp',

    # os
    'engine/src/service/platform.cpp',

    # debug
    'engine/src/debug/service.cpp',

    # logging
    'engine/src/log/service.cpp',
    'engine/src/log/message.cpp',
    'engine/src/log/sinks.cpp',

    # util
    'engine/src/util/retry.cpp',
    'engine/src/util/time.cpp',

    # input
    'engine/src/input/service.cpp',
    'engine/src/input/input.cpp',
    'engine/src/input/win32-device.cpp',
    'engine/src/input/xinput-device.cpp',

    # threads
    'engine/src/threads/service.cpp',
    'engine/src/threads/queue.cpp',
    'engine/src/threads/exclude.cpp',
    'engine/src/threads/thread.cpp',
    'engine/src/threads/scheduler.cpp',
    'engine/src/threads/name.cpp',
    'engine/src/threads/mutex.cpp',

    # freetype
    'engine/src/service/freetype.cpp',

    # depot
    'engine/src/depot/service.cpp',
    'engine/src/depot/vfs.cpp',
    'engine/src/depot/font.cpp',
    'engine/src/depot/image.cpp',

    # audio
    'engine/src/audio/service.cpp',
    'engine/src/audio/vorbis.cpp',
    'engine/src/audio/audio.cpp',

    # rhi
    'engine/src/rhi/service.cpp',
    'engine/src/rhi/render.cpp',
    'engine/src/rhi/vendor.cpp',

    # render graph
    'engine/src/render/service.cpp',
    'engine/src/render/render.cpp',
    'engine/src/render/graph.cpp',
    'engine/src/render/assets.cpp',
]

inc = [
    'engine/include',
    'vendor/include'
]

deps = [
    # debug
    dbghelp,

    # core
    fmt,

    # profiling
    tracy,

    # input
    xinput,

    # threads
    moodycamel, threads,

    # config
    tomlpp,

    # assets
    stb, harfbuzz, freetype2, vorbis, vorbisfile,

    # render
    directx
]

# TODO: these should be plugins of some form

### ryzenmonitor service

src += [ 'vendor/src/ryzenmonitor/common.cpp' ]

if opt_ryzenmonitor.enabled()
    netapi = dependency('win32_netapi')
    src += [
        'vendor/src/ryzenmonitor/service.cpp',
        'vendor/src/ryzenmonitor/devices.cpp'
    ]
    deps += [ ryzenmonitor, netapi ]
endif

### gameruntime service

if opt_gameruntime.enabled()
    src += [ 'vendor/src/gameruntime/service.cpp' ]
    deps += [ gameruntime ]
endif

libengine = library('engine', src,
    include_directories : inc,
    dependencies : deps,
    cpp_args : args
)

engine = declare_dependency(
    link_with : libengine,
    include_directories : inc,
    dependencies : deps,
    compile_args : args
)

###
### dear imgui
###

src = [
    # dear imgui
    'editor/src/imgui/imgui.cpp',
    'editor/src/imgui/imgui_draw.cpp',
    'editor/src/imgui/imgui_widgets.cpp',
    'editor/src/imgui/imgui_demo.cpp',
    'editor/src/imgui/imgui_tables.cpp',

    # dear imgui freetype
    'editor/src/imgui/misc/imgui_freetype.cpp',

    # dear imgui backends
    'editor/src/imgui/backends/imgui_impl_win32.cpp',
    'editor/src/imgui/backends/imgui_impl_dx12.cpp',

    # dear imgui extensions
    'editor/src/implot/implot.cpp',
    'editor/src/implot/implot_items.cpp',
    'editor/src/implot/implot_demo.cpp'
]

libimgui = library('imgui', src,
    include_directories : [ 'editor/include' ],
    dependencies : [ engine ],
    cpp_args : [
        '/wd4702', # disable warning C4702: unreachable code
        '/wd4127'  # disable warning C4127: conditional expression is constant
    ]
)

imgui = declare_dependency(
    link_with : libimgui,
    include_directories : [ 'editor/include' ],
    dependencies : [ engine ]
)

###
### editor
###

tinyobj = subproject('tinyobj').get_variable('tinyobj')

fs = import('fs')

shader_mode = '--default'
vs_output = [ '@BASENAME@.vs.cso' ]
ps_output = [ '@BASENAME@.ps.cso' ]

if debug_render.allowed()
    vs_output += [ '@BASENAME@.vs.pdb' ]
    ps_output += [ '@BASENAME@.ps.pdb' ]
    shader_mode = '--debug'
endif

hlsl_compile_script = find_program(files(script_dir/'compile-hlsl.py'), native : true)
copy_asset_script = find_program(files(script_dir/'copy-asset.py'), native : true)

hlsl_vs = generator(hlsl_compile_script,
    output : vs_output,
    arguments : [ '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'vs', '6_0', shader_mode ]
)

hlsl_ps = generator(hlsl_compile_script,
    output : ps_output,
    arguments : [ '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'ps', '6_0', shader_mode ]
)

# hlsl_cs = generator(hlsl_compile_script,
#     output : '@BASENAME@.cs.cso',
#     arguments : [ '@INPUT@', '@BUILD_DIR@/@BASENAME@', 'cs', '6_0' ]
# )

copy = generator(copy_asset_script,
    output : '@PLAINNAME@',
    arguments : [ '@INPUT@', '@OUTPUT@' ]
)

# TODO: really need a depot config soon
fs.copyfile('data/config/MicrosoftGame.Config')

src = [
    'editor/src/main.cpp',

    # render graph objects
    'editor/src/editor/graph/assets.cpp',
    'editor/src/editor/graph/mesh/obj.cpp',

    # render graph passes
    'editor/src/editor/graph/post.cpp',
    'editor/src/editor/graph/gui.cpp',

    # editor ui
    'editor/src/editor/service.cpp',
    'editor/src/editor/ui/ui.cpp',

    # ui components
    'editor/src/editor/ui/components/buffer.cpp',

    # uis for services
    'editor/src/editor/ui/panels/ryzenmonitor.cpp',
    'editor/src/editor/ui/panels/gameruntime.cpp',
    'editor/src/editor/ui/panels/world.cpp',
    'editor/src/editor/ui/panels/threads.cpp',
    'editor/src/editor/ui/panels/logging.cpp',
    'editor/src/editor/ui/panels/depot.cpp',
    'editor/src/editor/ui/panels/config.cpp',
    'editor/src/editor/ui/panels/audio.cpp',

    # game framework
    'editor/src/game/service.cpp',

    # ecs
    'editor/src/game/ecs/world.cpp',
    'editor/src/game/ecs/storage.cpp',
    'editor/src/game/ecs/typeinfo.cpp',

    # game rendering
    'editor/src/game/render/hud.cpp',
    'editor/src/game/render/scene.cpp',

    # game ui
    'editor/src/game/render/hud/layout.cpp',

    # shaders
    hlsl_vs.process('data/shaders/blit.hlsl'),
    hlsl_ps.process('data/shaders/blit.hlsl'),

    hlsl_vs.process('data/shaders/object.hlsl'),
    hlsl_ps.process('data/shaders/object.hlsl'),

    hlsl_vs.process('data/shaders/hud.hlsl'),
    hlsl_ps.process('data/shaders/hud.hlsl'),

    #hlsl_cs.process('data/shaders/mipmap.hlsl'),

    # fonts
    copy.process('data/fonts/SwarmFace-Regular.ttf'),

    # textures
    copy.process('data/images/assets/default.png'),
    copy.process('data/images/assets/uv-coords.png'),
    copy.process('data/images/assets/player.png'),
    copy.process('data/images/assets/cross.png'),
    copy.process('data/images/assets/alien.png'),
    copy.process('data/images/assets/death.png'),
    copy.process('data/images/assets/meme.jpg'),

    # models
    copy.process('data/models/default.model'),
    copy.process('data/models/cross.model'),
    copy.process('data/models/grid.model'),
    copy.process('data/models/ship.model'),
    copy.process('data/models/alien.model'),
    copy.process('data/models/plane.model'),

    copy.process('data/models/bullet.model'),
    copy.process('data/models/egg-small.model'),
    copy.process('data/models/egg-medium.model'),
    copy.process('data/models/egg-large.model'),

    # libraries
    copy.process('data/libs/warp/d3d10warp.dll'),
    copy.process('data/libs/warp/d3d10warp.pdb'),

    copy.process('data/libs/agility/D3D12Core.dll'),
    copy.process('data/libs/agility/D3D12Core.pdb'),
    copy.process('data/libs/agility/d3d12SDKLayers.dll'),
    copy.process('data/libs/agility/d3d12SDKLayers.pdb'),

    # config
    copy.process('data/config/MicrosoftGame.Config'),

    # gdk assets
    copy.process('data/images/store/logo44x44.png'),
    copy.process('data/images/store/logo100x100.png'),
    copy.process('data/images/store/logo150x150.png'),
    copy.process('data/images/store/logo480x480.png'),
    copy.process('data/images/store/splash.png'),

    # sfx
    copy.process('data/audio/sounds/pew.ogg'),
    copy.process('data/audio/sounds/alien_kill.ogg'),
    copy.process('data/audio/sounds/damage_hit.ogg'),
    copy.process('data/audio/sounds/game_over.ogg'),

    copy.process('data/audio/sounds/egg_spawn.ogg'),
    copy.process('data/audio/sounds/egg_grow_medium.ogg'),
    copy.process('data/audio/sounds/egg_grow_large.ogg'),
    copy.process('data/audio/sounds/egg_kill.ogg'),
    copy.process('data/audio/sounds/egg_hatch.ogg'),

    # music
    copy.process('data/audio/sounds/swarm1.ogg'),
    copy.process('data/audio/sounds/swarm2.ogg'),
    copy.process('data/audio/sounds/swarm3.ogg'),
    copy.process('data/audio/sounds/swarm4.ogg'),
    copy.process('data/audio/sounds/swarm5.ogg'),

    # imgui config
    copy.process('data/config/imgui.ini')
]

deps = [
    # general
    engine,

    # imgui
    imgui,

    # render
    tinyobj
]

args = [
    '-DEDITOR=1', # we are the editor
]

editor = executable('editor', src,
    dependencies : deps,
    include_directories : [ 'editor/include' ],
    cpp_args : args,
    gui_app : gui_app.enabled()
)

###
### client
###

src = [ 'client/src/main.cpp' ]

client = executable('client', src,
    dependencies : engine,
    include_directories : [ 'client/include' ],
    cpp_args : [ '-DCLIENT=1' ],
    gui_app : gui_app.enabled()
)

if msstore_package.enabled()
    # mkdir msstore-bundle
    # makepkg pack /f data/config/layout.xml /lt /d build /nogameos /pc /pd msstore-bundle
    # wdapp install msstore-bundle\32190Simcoe.Swarm1983_1.0.0.0_x64__7vy8vpnrb1vvr.msixvc /bootstrapper
endif

if steam_package.enabled()
    # mkdir steam-bundle
    # TODO: how does steam want the files laid out?
endif

build_summary = {
    'Services': {
        'Microsoft XGameRuntime': opt_gameruntime.enabled(),
        'Ryzen Monitoring SDK': opt_ryzenmonitor.enabled(),
        'Tracy profiler': opt_tracy
    },
    'Config': {
        'Build type': buildtype,
        'Engine debug': is_debug,
        'Threads debug': debug_threads.allowed(),
        'Render debug': debug_render.allowed(),
        'Audio debug': debug_audio.allowed(),
        'Windows console app': not gui_app.enabled()
    },
    'Profiling': {
        'Startup': profile_startup.allowed()
    },
    'Deploy': {
        'Microsoft store': msstore_package.enabled(),
        'Steam': steam_package.enabled()
    }
}

foreach section_title, section : build_summary
    summary(section, bool_yn : true, section : section_title)
endforeach
