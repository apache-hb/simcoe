project('fastgltf', 'cpp')

simdjson = subproject('simdjson').get_variable('simdjson')

src = [ 'src/base64.cpp', 'src/fastgltf.cpp' ]
inc = [ 'include' ]
dep = [ simdjson ]

libfastgltf = library('fastgltf', src,
    include_directories : inc,
    dependencies : dep
)

fastgltf = declare_dependency(
    link_with : libfastgltf,
    include_directories : inc,
    dependencies : dep
)
