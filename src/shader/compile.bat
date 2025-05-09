glslc.exe triangle.vert -o triangle.vert.spv
glslc.exe triangle.frag -o triangle.frag.spv

@REM glslangValidator -V triangle.vert -o triangle.vert.spv
@REM glslangValidator -V triangle.frag -o triangle.frag.spv