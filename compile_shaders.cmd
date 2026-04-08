cd shaders/
slangc tri.slang -entry vs_main -stage vertex -target spirv -o tri.vs.spv
slangc tri.slang -entry fs_main -stage pixel -target spirv -o tri.ps.spv
