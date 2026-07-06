# elevator_project

## Floor Controller SW

### Building

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

The other thing to remember when configuring a build is that the source code is the same between the floor controllers and the car controller. Thus you must set the `NODE_ID` at the preset stage. 

For example:

```bash
cmake --preset Debug -DNODE_ID=car 
```

Valid values are: `car, floor1, floor2, floor3`

Another option that may be useful to set when debugging is the verbosity level of the UART logging.

```bash
cmake --preset Debug -DVERBOSITY=low 
```

Valid values are: `none, low, medium, high`. These levels are set when you call the debug logging function `dbglog()` by passing the first parameter as `LVL1`, `LVL2` , or `LVL3`. These messages will be compiled in or ignored according to the value of VERBOSITY.

>[!note]
>Verbosity has no effect in release builds as debug logging is disabled entirely. Debug logging can be disabled in Debug builds with `-DVERBOSITY=none`.

### Flashing

The build process will output a `.elf` file. This cannot be directly flashed to the STM32 because of metadata that ST-Link does not expect. It must first be converted to a `.bin` file using the toolchain specific `objcopy` (`arm-none-eabi-objcopy`). 

```bash
arm-none-eabi-objcopy -O binary FloorController.elf FloorController.bin
```

There is a phony target to run the file conversion and the flash using `st-flash`. 

```bash
cmake --build build/Debug --target flash
```

Alternatively the flash can be done with the gui `ST-link` utility.

#### References

- [st-flash man page](https://man.archlinux.org/man/st-flash.1.en)
- [STM32CubeMX CMake integration](https://community.st.com/t5/stm32-mcus/cmake-integration-in-stm32cubemx-and-usage-in-stm32cubeide-for/ta-p/849360)
- [UART communications with HAL](https://controllerstech.com/stm32-uart-1-configure-uart-transmit-data/)
- [VS Code setup](https://medium.com/@lixis630/getting-started-to-code-embedded-c-on-stm32-e90e7910b2c)
