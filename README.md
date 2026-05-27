# elevator_project

## Floor Controller SW

The floor controller SW is written in C and is built for the STM32F303RE with a HAL using
STM32CubeMX for codegen, and CMake as the build system. 

To add another build (currently there is release and debug) add an entry in the `CMakePresets.json` table and run the following command from the FloorController folder.

```bash
cmake --preset <preset>
```

There are 3 presets, which are defined in the `CMakePresets.json` file.

1. Default (Basic, for deriving others - not to be used)
2. Debug (No optimizations, debug symbols)
3. Release (Optimized, no debug symbols, NDEBUG defined for defining debug-only code
   sections)

The presets also prevent the need for defining the toolchain manually. If no preset is used, the system compiler will be used, which will result in linker/assembler errors. The toolchain can also be specified manually.

```bash
cmake -B build/<name> -S . -DCMAKE_TOOLCHAIN_FILE=toolchainfile.cmake
```


