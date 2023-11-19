from subprocess import run
from sys import argv
import os

# usage: compile-hlsl.py <file> <output> <targets> <model> [--debug]
# <file> is the input file
# <output> is the output file
# <targets> is the shader targets, comma separated
#           vs,ps,gs,hs,ds,cs
# <model> is the model to use, 6_0 to 6_6

dxc = 'dxc.exe'
file = argv[1]
output = argv[2]
targets = argv[3]
shader_model = argv[4]
debug = '--debug' in argv

cwd = os.getcwd()

for target in targets.split(','):
    entry = f'{target}Main'
    target_model = f'{target}_{shader_model}'
    output_name = f'{output}.{target}.cso'
    args = [ dxc, '-T' + target_model, '-E' + entry, '-Fo' + output_name, '-WX', '-Ges' ]
    if debug:
        # TODO: pix recommends letting dxc naming the pdb by itself, but currently we cant manage this
        args += [ '-Zi', '-DDEBUG=1', '-Qembed_debug', '/Fd', f'{output}.{target}.pdb' ] 
    else:
        args += [ '-O3' ]
    args += [ file ]

    print(' '.join(args))

    result = run(args)
    if result.returncode != 0:
        print(f'Error compiling {target} shader')
        exit(result.returncode)
