import os

compile_flags = """-I.
-IC:/msys64/ucrt64/include
-isystem
C:/msys64/ucrt64/include
-isystem
C:/msys64/ucrt64/lib/gcc/x86_64-w64-mingw32/16.2.0/include
-isystem
C:/msys64/ucrt64/lib/gcc/x86_64-w64-mingw32/16.2.0/include-fixed
--target=x86_64-w64-mingw32
-std=c11
-D_WIN32
-D_WIN32_WINNT=0x0601
-Wno-builtin-definition
-Wno-builtin-requires-header
-Wno-everything
"""

clangd_yaml = """CompileFlags:
  Add:
    - "-I."
    - "-IC:/msys64/ucrt64/include"
    - "-isystem"
    - "C:/msys64/ucrt64/include"
    - "-isystem"
    - "C:/msys64/ucrt64/lib/gcc/x86_64-w64-mingw32/16.2.0/include"
    - "-isystem"
    - "C:/msys64/ucrt64/lib/gcc/x86_64-w64-mingw32/16.2.0/include-fixed"
    - "--target=x86_64-w64-mingw32"
    - "-std=c11"
    - "-D_WIN32"
    - "-D_WIN32_WINNT=0x0601"
    - "-Wno-builtin-definition"
    - "-Wno-builtin-requires-header"
    - "-Wno-everything"

Diagnostics:
  Suppress:
    - "builtin_definition"
    - "unused-includes"
    - "pp_file_not_found"
    - "pp_file_not_found_angled_include_not_fatal"
    - "pp_file_not_found_use_quotes"
"""

flag_paths = [
    'C:/msys64/ucrt64/include/compile_flags.txt',
    'C:/msys64/ucrt64/compile_flags.txt',
    'C:/msys64/compile_flags.txt'
]

for p in flag_paths:
    try:
        with open(p, 'w', encoding='utf-8') as f:
            f.write(compile_flags)
        print('Wrote', p)
    except Exception as e:
        print('Error writing', p, e)

yaml_paths = [
    'C:/msys64/ucrt64/include/.clangd',
    'C:/msys64/ucrt64/.clangd',
    'C:/msys64/.clangd',
    'C:/Users/singh/AppData/Local/clangd/config.yaml',
    'C:/Users/singh/.clangd'
]

for p in yaml_paths:
    try:
        os.makedirs(os.path.dirname(p), exist_ok=True)
        with open(p, 'w', encoding='utf-8') as f:
            f.write(clangd_yaml)
        print('Wrote', p)
    except Exception as e:
        print('Error writing', p, e)
