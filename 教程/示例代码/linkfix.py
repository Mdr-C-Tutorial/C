# coding=utf-8
from os import walk
from os.path import join
for path, _, files in walk('.'):
    for file in files:
        if not file.endswith('.md'):
            with open(join(path, file) + '.md', 'w') as f:
                f.write(f"[{file}](./{file} ':include')")