@REM glslc.exe triangle.vert -o triangle.vert.spv
@REM glslc.exe triangle.frag -o triangle.frag.spv

glslangValidator -V triangle.vert -o triangle.vert.spv
glslangValidator -V triangle.frag -o triangle.frag.spv