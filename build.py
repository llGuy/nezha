#!/usr/bin/env python3

import os
import subprocess

project_dir = os.getcwd()
build_dir = 'build'
shader_dir = 'res/glsl'

if os.path.exists(build_dir):
  os.chdir(build_dir)
  subprocess.run('make', shell=True)
  os.chdir(project_dir)
else:
  os.makedirs(build_dir)
  os.chdir(build_dir)
  subprocess.run('cmake ..', shell=True)
  subprocess.run('make', shell=True)
  os.chdir(project_dir)

os.chdir(shader_dir)
subprocess.run('make', shell=True)
os.chdir(project_dir)
