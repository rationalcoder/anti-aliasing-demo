#!/bin/bash
/c/cloc/cloc-1.64.exe `ls | grep -v "stb_" | grep -v "imgui" | grep -v "run_tree" | grep -v "\.pro.*" | grep -v "middleware" | grep -v "build"` run_tree/demo/basic.vs run_tree/demo/*.fs
