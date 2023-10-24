project('directx', 'cpp')

libs = [ 'd3d12.lib', 'dxgi.lib', 'dxguid.lib' ]

libd3dx12 = library('d3dx12', 'src/d3dx12/d3dx12_property_format_table.cpp',
    link_args : libs,
    include_directories : [ 'include/directx', 'include/directx/d3dx12' ]
)

directx = declare_dependency(
    link_with : [ libd3dx12 ],
    link_args : libs,
    include_directories : 'include'
)
