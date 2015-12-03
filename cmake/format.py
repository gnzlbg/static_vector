#!/usr/bin/env python
# Copyright Gonzalo Brito Gadeschi 2015
# Distributed under the Boost Software License, Version 1.0.
# (See accompanying file LICENSE.md or copy at http://boost.org/LICENSE_1_0.txt)
"""Recursively formats C/C++ files using clang-format

Usage:
  format.py <clang_format_path> <project_src_path> [options]
  format.py -h | --help
  format.py --version

  <clang_format_path>  Path to clang-format's binary.
  <project_src_path>   Path to the project's source directory.

Options:
  -h --help  Show this screen.
  --verbose  Verbose output.
  --apply    Applies format (by default it only checks the format).

"""
from docopt import docopt
import os
import subprocess

file_extensions = ['.c', '.h', '.cpp', '.cc', '.cxx', 
                   '.hpp', '.hh', '.hxx', '.c++', '.h++']

def run_clang_format(clang_format, path, apply_format, verbose):
    if apply_format:
        cmd = clang_format + ' -style=file -i ' + path
    else:
        cmd = clang_format + ' -style=file ' + path + ' | diff -u ' + path + ' - '

    if verbose: print('[clang_format cmd]: "{0}"'.format(cmd))

    p = subprocess.Popen(cmd, universal_newlines=False, shell=True,
                         stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    
    out, err = p.communicate()
    if len(out) > 0:
        print out
    if len(err) > 0:
        print err

    if not p.returncode == 0 or len(err) > 0 or len(out) > 0:
        if verbose: print("failed!")
        return False
    else:
        if verbose: print("success!")
        return True

def run(clang_format_path, file_paths, apply_format, verbose):
    result = True
    for p in file_paths:
        _, ext = os.path.splitext(p)
        if ext in file_extensions:
            r = run_clang_format(clang_format_path, p, apply_format, verbose)
            if not r:
                result = False
    return result

def main():
    args = docopt(__doc__)

    clang_format_path = args['<clang_format_path>']
    project_src_path = args['<project_src_path>']
    verbose = args['--verbose']

    files = subprocess.check_output(['git', 'ls-tree', '--full-tree', '-r', 'HEAD', project_src_path])
    file_paths = [os.path.join(project_src_path,f.split('\t')[1]) for f in files.splitlines()] 

    apply_format = args['--apply']

    result = run(clang_format_path, file_paths, apply_format, verbose)

    if apply_format: # If format was applied: check format and use that as result
        result = run(clang_format_path, file_paths, False, verbose)

    if verbose:
        if result:
            print("finished with success!") 
        else:
            print("finished with failed!")
    if result:
        exit(0)
    else:
        exit(1)
        

if __name__ == '__main__':
    main()
