project('simdjson', 'cpp')

src = [ 'src/simdjson.cpp' ]
inc = [ 'include' ]

libsimdjson = library('simdjson', src,
    include_directories : inc
)

simdjson = declare_dependency(
    include_directories : inc,
    link_with : libsimdjson
)
