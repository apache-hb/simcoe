project('tinyobj', 'cpp')

src = [ 'src/loader.cpp' ]
inc = [ 'include' ]

libtinyobj = library('tinyobj', src,
    include_directories : inc
)

tinyobj = declare_dependency(
    link_with : libtinyobj,
    include_directories : inc
)
