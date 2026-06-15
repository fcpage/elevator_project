# Modern Engineering C++ Style Guide

## Purpose

This guide defines a practical C++ style for engineering software, embedded systems, firmware-adjacent tooling, and desktop applications.

The guide is primarily inspired by the Microsoft C++ Core Guidelines style of modern, safe, readable C++, while selectively borrowing useful conventions from other large-scale C++ styles where they improve clarity. It is intentionally not a pure Microsoft, Google, LLVM, or embedded-only guide.

The main goal is simple:

> Write C++ that is easy to read, safe by default, practical for engineering work, and honest about the difference between desktop software and embedded firmware.

This guide applies to:

- Desktop C++ applications
- Engineering tools
- Simulation programs
- Embedded C++ projects
- C-compatible firmware modules
- Mixed C/C++ codebases

It does **not** force embedded restrictions onto desktop software.

Desktop and general software should use modern C++ fully when available. Embedded code should use modern C++ carefully, with attention to determinism, memory usage, toolchain support, and hardware transparency.

---

## 1. Language Versions

### 1.1 Preferred C++ Version

Use the most modern stable C++ version supported by the project toolchain.

For desktop and general applications:

- Prefer the newest practical C++ standard available.
- C++20 and newer are encouraged when supported.
- C++23 features may be used when compiler and build support are reliable.

For embedded systems:

- Use the newest C++ version reliably supported by the target compiler.
- The oldest accepted C++ version is **C++11**.
- Do not use a modern feature merely because it exists. Use it when it improves safety, readability, or maintainability without unacceptable runtime or binary-size cost.

### 1.2 C Version

For C code:

- The minimum accepted standard is **C99**.
- C code should remain clear, portable where practical, and compatible with C++ integration when needed.

---

## 2. Core Principles

### 2.1 Prefer Clarity Over Cleverness

Code should be understandable after a direct read-through.

Avoid clever expressions, dense template tricks, hidden control flow, or abstractions that require the reader to chase through several files to understand basic behaviour.

Good:

```cpp
if (adcCount > kMaximumSafeAdcCount)
{
    faultManager.raiseFault(FaultCode::AdcOverRange);
}
```

Avoid:

```cpp
faultManager.raiseIf(adcCount > limits.adc().safe().max(), FaultCode::AdcOverRange);
```

The second example may be acceptable in a high-level desktop framework, but it is often too indirect for embedded or hardware-facing code.

### 2.2 Make Ownership Clear

Resource ownership should be obvious.

Prefer RAII in C++ when appropriate:

```cpp
class SerialConnection
{
public:
    explicit SerialConnection(std::string portName);
    ~SerialConnection();

    bool open();
    void close();

private:
    std::string portName_;
    bool isOpen_ = false;
};
```

For embedded systems, RAII may still be useful, but avoid constructors or destructors that hide expensive hardware operations unless the behaviour is clearly documented.

### 2.3 Avoid Hidden Runtime Cost

Especially in embedded code, avoid operations that hide:

- dynamic allocation
- blocking I/O
- hardware register writes
- interrupt state changes
- large copies
- expensive conversions
- mutex locks
- heap use
- exception paths

If a function performs a significant operation, its name and documentation should make that clear.

Good:

```cpp
flashStorage.eraseSector(sectorIndex);
```

Risky:

```cpp
flashStorage.reset(sectorIndex);
```

### 2.4 Separate Desktop Rules From Embedded Rules

Desktop applications should be allowed to use modern C++ normally.

Embedded code should be more restrictive.

Do not ban useful C++ features globally just because they are unsuitable for some firmware targets.

---

## 3. File Naming and File Organization

### 3.1 Preferred File Extensions

Preferred:

```text
.cpp
.hpp
```

Accepted:

```text
.cpp
.h
```

Use `.hpp` for C++ headers unless the project has a reason to use `.h`.

Use `.h` when:

- the header must be included from C
- the project already follows `.h` convention
- the file exposes a C-compatible interface

### 3.2 One File, One Main Purpose

Each source file should have one clear responsibility.

Good:

```text
uart_port.cpp
uart_port.hpp
adc_sampler.cpp
adc_sampler.hpp
tag_parser.cpp
tag_parser.hpp
```

Avoid:

```text
utilities.cpp
misc.cpp
drivers.cpp
everything.cpp
```

General utility files are allowed, but they should not become dumping grounds.

### 3.3 Header Files

Header files should contain:

- public declarations
- public type definitions
- public constants when needed
- short inline functions only when justified
- Doxygen comments for public APIs

Header files should avoid:

- unnecessary includes
- large implementation blocks
- hidden global state
- complex logic
- project-wide macros unless unavoidable

### 3.4 Source Files

Source files should contain:

- implementation details
- private helper functions
- file-local constants
- internal namespace content
- hardware register logic where appropriate

---

## 4. Top-of-File Comments

Top-of-file comments are optional but encouraged for important files, hardware modules, public APIs, generated files, or complex implementation units.

Use a compact block format:

```cpp
/******************************************************************
* uart_port.cpp - UART Peripheral Driver
* Author: *name*
* Last Modified: 2026-08-05
* @brief Implements UART initialization and byte-level transmit logic.
*
* Related Files:
* - gpio_config.cpp
* - clock_config.cpp
******************************************************************/
```

### 4.1 Required Fields

When used, the file header should include:

- file name
- short purpose/title
- author
- last modified date
- brief high-level description

### 4.2 Optional Fields

Optional fields may include:

- related source files
- hardware target
- module notes
- generated-file warning
- safety notes

Avoid long explanations in the file header. Keep it terse and high level.

---

## 5. Naming Conventions

### 5.1 General Naming Philosophy

Names should be descriptive, readable, and direct.

Prefer names that explain engineering intent rather than implementation trivia.

Good:

```cpp
uint32_t measuredAdcCount;
float motorDutyCycle;
bool isFaultActive;
```

Avoid:

```cpp
uint32_t val;
float x;
bool flag;
```

Short names are acceptable for very small scopes:

```cpp
for (int i = 0; i < sampleCount; ++i)
{
    samples[i] = 0;
}
```

### 5.2 Types

Use `PascalCase` for types.

```cpp
class MotorController;
struct AdcSample;
enum class FaultCode;
using SampleBuffer = std::array<uint16_t, 128>;
```

### 5.3 Functions and Methods

Use `camelCase` for functions and methods.

```cpp
void initializeHardware();
bool readAdcSample();
void setDutyCycle(float dutyCycle);
```

This applies especially to functional names: things that perform actions should read naturally as actions.

### 5.4 Variables

Use `camelCase` for local variables and function parameters.

```cpp
uint32_t currentSpeedRpm;
float measuredVoltage;
bool isSensorConnected;
```

### 5.5 Private Data Members

Use a trailing underscore for private class members.

```cpp
class MotorController
{
public:
    void setTargetSpeed(uint32_t targetSpeedRpm);

private:
    uint32_t targetSpeedRpm_ = 0;
    bool isEnabled_ = false;
};
```

This avoids ambiguity between parameters and stored state.

```cpp
MotorController::MotorController(uint32_t targetSpeedRpm)
    : targetSpeedRpm_(targetSpeedRpm)
{
}
```

### 5.6 Constants

Use Google-style `k` prefix with `PascalCase` for constants.

```cpp
constexpr uint32_t kUartBaudRate = 115200;
constexpr uint16_t kMaximumAdcCount = 4095;
constexpr float kReferenceVoltage = 3.3f;
```

Use constants instead of magic numbers.

Avoid:

```cpp
if (adcCount > 4095)
{
    return false;
}
```

Prefer:

```cpp
if (adcCount > kMaximumAdcCount)
{
    return false;
}
```

### 5.7 Enumerations

Use `enum class` in C++ unless there is a specific reason to use a plain enum.

```cpp
enum class MotorState
{
    Disabled,
    Starting,
    Running,
    Faulted
};
```

Prefer clear enum values without redundant prefixes when scoped by the enum type.

Good:

```cpp
MotorState::Running
```

Avoid:

```cpp
MotorState::MotorStateRunning
```

### 5.8 Macros

Avoid macros when a C++ feature can do the job safely.

Prefer:

```cpp
constexpr uint32_t kBufferSize = 256;
```

Instead of:

```cpp
#define BUFFER_SIZE 256
```

Macros are acceptable when required for:

- conditional compilation
- compiler attributes
- hardware register definitions
- C compatibility
- platform-specific build configuration

Macro names should use `UPPER_SNAKE_CASE`.

```cpp
#define ENABLE_DIAGNOSTIC_LOGGING 1
```

---

## 6. Formatting

### 6.1 Braces

Use braces for all control statements, even single-line bodies.

```cpp
if (isEnabled)
{
    updateMotor();
}
```

Avoid:

```cpp
if (isEnabled)
    updateMotor();
```

This prevents bugs during later edits.

### 6.2 Indentation

Use consistent indentation across the project.

Recommended:

- 4 spaces
- no tabs for indentation

```cpp
void update()
{
    if (isReady)
    {
        processSample();
    }
}
```

### 6.3 Line Length

Prefer readable lines.

Recommended soft limit:

```text
100 to 120 characters
```

Do not contort code to satisfy a strict limit if it makes the result harder to read.

### 6.4 Blank Lines

Use blank lines to separate logical groups.

```cpp
bool MotorController::start()
{
    if (isFaulted_)
    {
        return false;
    }

    enablePowerStage();
    setDutyCycle(kStartupDutyCycle);

    isRunning_ = true;
    return true;
}
```

### 6.5 Pointer and Reference Style

Attach `*` and `&` to the type.

```cpp
int* samplePointer;
const AdcSample& sample;
```

This emphasizes that the type is pointer or reference qualified.

### 6.6 Include Order

Recommended include order:

1. matching header
2. C++ standard library headers
3. C standard library headers
4. third-party library headers
5. project headers

Example:

```cpp
#include "uart_port.hpp"

#include <array>
#include <cstdint>

#include "clock_config.hpp"
#include "gpio_config.hpp"
```

---

## 7. Classes and Structs

### 7.1 Use Classes for Behaviour

Use `class` when the type owns state and provides behaviour.

```cpp
class PwmOutput
{
public:
    explicit PwmOutput(TimerChannel channel);

    void setDutyCycle(float dutyCycle);
    void enable();
    void disable();

private:
    TimerChannel channel_;
    float dutyCycle_ = 0.0f;
    bool isEnabled_ = false;
};
```

### 7.2 Use Structs for Plain Data

Use `struct` for simple passive data.

```cpp
struct AdcSample
{
    uint16_t rawCount = 0;
    float voltage = 0.0f;
};
```

Avoid adding complex behaviour to structs. If a type starts owning behaviour, consider making it a class.

### 7.3 Constructor Rules

Use `explicit` for single-argument constructors unless implicit conversion is intended.

```cpp
class AdcChannel
{
public:
    explicit AdcChannel(uint8_t channelNumber);

private:
    uint8_t channelNumber_;
};
```

### 7.4 Initialization

Prefer member initialization and initializer lists.

```cpp
class MotorController
{
public:
    MotorController(PwmOutput& pwmOutput, GpioPin& enablePin)
        : pwmOutput_(pwmOutput),
          enablePin_(enablePin)
    {
    }

private:
    PwmOutput& pwmOutput_;
    GpioPin& enablePin_;
};
```

For embedded code, avoid constructors that silently perform hardware initialization unless that is the clear project convention.

---

## 8. Constants, Configuration, and Magic Numbers

### 8.1 Prefer `constexpr`

Use `constexpr` for compile-time constants.

```cpp
constexpr uint32_t kSystemClockHz = 72000000;
constexpr uint32_t kUartBaudRate = 115200;
constexpr size_t kSampleBufferLength = 256;
```

### 8.2 Use Named Configuration Values

Configuration values should be named.

```cpp
constexpr float kMinimumValidVoltage = 0.1f;
constexpr float kMaximumValidVoltage = 3.2f;
```

### 8.3 Hardware Constants

Hardware constants should clearly state the unit or meaning.

Good:

```cpp
constexpr uint32_t kAdcSampleRateHz = 20000;
constexpr uint32_t kControlLoopPeriodUs = 1000;
constexpr uint16_t kMaximumAdcCount = 4095;
```

Avoid:

```cpp
constexpr uint32_t kRate = 20000;
constexpr uint32_t kPeriod = 1000;
constexpr uint16_t kMax = 4095;
```

---

## 9. Error Handling

### 9.1 Desktop Applications

Desktop applications may use exceptions when appropriate.

Exceptions are acceptable for:

- unrecoverable construction failures
- invalid file formats
- failed external resource acquisition
- high-level application errors

Example:

```cpp
ProjectFile loadProjectFile(const std::filesystem::path& filePath)
{
    if (!std::filesystem::exists(filePath))
    {
        throw std::runtime_error("Project file does not exist.");
    }

    return parseProjectFile(filePath);
}
```

### 9.2 Embedded Systems

Embedded systems should usually avoid exceptions unless the platform explicitly supports them and the project permits them.

Prefer explicit return values:

```cpp
enum class DriverStatus
{
    Ok,
    InvalidConfiguration,
    HardwareTimeout,
    PeripheralDisabled
};

DriverStatus initializeUart(uint32_t baudRate);
```

Or:

```cpp
bool initializeUart(uint32_t baudRate);
```

Use simple `bool` only when failure does not need detailed explanation.

### 9.3 Avoid Silent Failure

Do not ignore return values from important operations.

Bad:

```cpp
flashStorage.writeBlock(address, data);
```

Good:

```cpp
const bool writeSucceeded = flashStorage.writeBlock(address, data);

if (!writeSucceeded)
{
    faultManager.raiseFault(FaultCode::FlashWriteFailed);
}
```

---

## 10. Memory Management

### 10.1 Desktop Applications

Desktop applications may use standard containers and dynamic allocation normally.

Prefer standard library types:

```cpp
std::vector<TagRecord> tagRecords;
std::string projectName;
std::unique_ptr<SimulationNode> rootNode;
```

Avoid manual memory management unless necessary.

```cpp
auto buffer = std::make_unique<std::array<uint8_t, 1024>>();
```

### 10.2 Embedded Systems

Embedded systems should restrict memory allocation based on target constraints.

Recommended default for firmware:

- avoid heap allocation after startup
- prefer static allocation
- prefer fixed-size buffers
- document large buffers
- avoid unbounded containers
- avoid hidden allocations in control loops

Good:

```cpp
std::array<uint16_t, kSampleBufferLength> sampleBuffer;
```

Risky in firmware:

```cpp
std::vector<uint16_t> sampleBuffer;
sampleBuffer.push_back(newSample);
```

A `std::vector` may be acceptable during initialization or on embedded Linux, but it should not be used casually in hard real-time paths.

---

## 11. Standard Library Usage

### 11.1 Desktop Applications

Use the standard library fully when it improves clarity.

Recommended:

```cpp
std::vector
std::string
std::filesystem
std::optional
std::variant
std::unique_ptr
std::shared_ptr
std::span
std::array
std::chrono
```

### 11.2 Embedded Systems

Use standard library features selectively.

Generally safe and useful:

```cpp
std::array
std::span
std::optional
std::pair
std::tuple
std::byte
std::chrono
```

Use with caution:

```cpp
std::vector
std::string
std::function
std::map
std::unordered_map
std::shared_ptr
iostreams
```

Avoid in tight embedded paths unless measured and justified:

```cpp
std::function
std::regex
std::iostream
std::locale
```

---

## 12. Embedded-Specific Rules

### 12.1 Determinism Matters

Embedded control code should have predictable timing and memory behaviour.

Avoid:

- hidden allocation
- recursion unless tightly bounded
- unbounded loops
- blocking operations inside control loops
- exceptions in firmware unless explicitly allowed
- dynamic polymorphism in hot paths unless justified

### 12.2 Hardware Side Effects Must Be Obvious

Functions that write hardware registers, change pin states, enable interrupts, erase flash, or start DMA should make that clear.

Good:

```cpp
interruptController.disableGlobalInterrupts();
flashMemory.eraseSector(sectorIndex);
dmaController.startTransfer(source, destination, byteCount);
```

Bad:

```cpp
systemManager.prepare();
memoryManager.clean();
transferManager.run();
```

### 12.3 Interrupt Code

Interrupt service routines should be:

- short
- deterministic
- non-blocking
- allocation-free
- careful with shared state

Example:

```cpp
extern "C" void TIM2_IRQHandler(void)
{
    timerDriver.clearUpdateInterruptFlag();
    controlLoopRequestFlag = true;
}
```

Avoid doing heavy work directly inside the ISR.

### 12.4 Volatile

Use `volatile` for memory-mapped registers and variables shared with interrupt context when appropriate.

Do not use `volatile` as a substitute for synchronization in desktop multithreaded code.

### 12.5 Register Access

Direct register access is acceptable in low-level driver code.

```cpp
GPIOA->BSRR = kStatusLedPinMask;
```

Higher-level code should normally use a clear abstraction:

```cpp
statusLed.setHigh();
```

Both styles are acceptable at the correct layer.

---

## 13. Abstraction Rules

### 13.1 Abstraction Should Reveal Intent

Good abstraction makes intent clearer without hiding essential behaviour.

Good:

```cpp
statusLed.setHigh();
motorPwm.setDutyCycle(0.45f);
adcInput.readRawCount();
```

These calls clearly describe hardware intent.

### 13.2 Avoid Excessive Abstraction Layers

For basic hardware behaviour, avoid more than two layers of abstraction unless there is a strong reason.

Acceptable:

```cpp
statusLed.setHigh();
```

Internally:

```cpp
void GpioOutput::setHigh()
{
    port_->BSRR = pinMask_;
}
```

Too abstract:

```cpp
deviceManager.getOutputController()
             .getVisualIndicator()
             .applyState(OutputState::Asserted);
```

This hides a simple pin write behind too much structure.

### 13.3 Justified Deeper Abstraction

More abstraction may be justified for:

- simulation support
- test doubles
- multiple hardware variants
- desktop/embedded shared logic
- platform-independent business logic
- complex protocol handling

The deeper the abstraction, the clearer the interfaces and documentation must be.

---

## 14. Hardware-Facing Code

### 14.1 Keep Low-Level Hardware Code Contained

Hardware register manipulation should be isolated to driver or board-support layers.

Example structure:

```text
src/
    app/
        motor_control.cpp
    drivers/
        gpio_output.cpp
        pwm_output.cpp
        adc_input.cpp
    board/
        board_pins.cpp
        clock_config.cpp
```

Application code should express intent:

```cpp
coolingFan.setDutyCycle(0.60f);
statusLed.setHigh();
```

Driver code may express register-level behaviour:

```cpp
timer_->CCR1 = compareValue;
```

### 14.2 Prefer Explicit Units

Use units in names.

Good:

```cpp
uint32_t timeoutMs;
uint32_t sampleRateHz;
uint32_t pulseWidthUs;
float voltageV;
```

Avoid:

```cpp
uint32_t timeout;
uint32_t rate;
uint32_t pulse;
float value;
```

### 14.3 Avoid Surprising Hardware Work in Constructors

In embedded code, constructors should usually store configuration, not necessarily start hardware.

Prefer:

```cpp
UartPort uartPort(USART2);

uartPort.initialize(kUartBaudRate);
```

Be cautious with:

```cpp
UartPort uartPort(USART2, kUartBaudRate);
```

The second form may be acceptable, but only if the project convention allows constructors to configure hardware.

---

## 15. Documentation Comments

### 15.1 Public APIs Use Doxygen

Public classes, public functions, public structs, and public enums should use Doxygen comments when they are part of an API.

Example:

```cpp
/**
 * @brief Sets the PWM output duty cycle.
 *
 * The duty cycle is clamped to the valid range of 0.0 to 1.0 before being
 * written to the timer compare register.
 *
 * @param dutyCycle Requested duty cycle from 0.0 to 1.0.
 */
void setDutyCycle(float dutyCycle);
```

### 15.2 Comments Should Explain Intent

Comments should explain:

- why code exists
- hardware assumptions
- timing assumptions
- safety constraints
- non-obvious transformations
- unusual edge cases

Avoid comments that merely repeat the code.

Bad:

```cpp
// Increment i by one.
++i;
```

Good:

```cpp
// The first ADC sample after channel switching is discarded because the
// sample-and-hold capacitor may still contain charge from the previous channel.
discardNextSample = true;
```

### 15.3 Generated Code

Generated files must clearly say they are generated.

```cpp
/******************************************************************
* generated_tag_definitions.cpp - Generated Tag Definitions
* Author: Code Generator
* Last Modified: 2026-08-05
* @brief Auto-generated tag constants. Do not edit manually.
******************************************************************/
```

---

## 16. C and C++ Interoperability

### 16.1 C-Compatible Headers

Headers intended for C and C++ should use `extern "C"` guards.

```c
#pragma once

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

bool uart_initialize(uint32_t baud_rate);
bool uart_write_byte(uint8_t value);

#ifdef __cplusplus
}
#endif
```

### 16.2 C Naming

For pure C modules, use `snake_case` when following typical C conventions, especially for C-compatible APIs.

```c
bool uart_initialize(uint32_t baud_rate);
bool uart_write_byte(uint8_t value);
```

For C++ code, use `camelCase`.

```cpp
bool initializeUart(uint32_t baudRate);
bool writeUartByte(uint8_t value);
```

### 16.3 C99 Rules

For C99 code:

- use fixed-width integer types where hardware size matters
- avoid unnecessary macros
- keep ownership clear
- prefer small functions
- document hardware assumptions
- avoid global mutable state unless required by hardware or ISR structure

---

## 17. Templates

### 17.1 Desktop Applications

Templates are allowed when they improve type safety, reuse, or performance.

Good desktop use:

```cpp
template <typename RecordType>
std::vector<RecordType> loadRecordsFromCsv(const std::filesystem::path& filePath);
```

### 17.2 Embedded Systems

Templates are allowed in embedded code, but should be used carefully.

Good embedded use:

```cpp
template <size_t BufferLength>
class RingBuffer
{
public:
    bool push(uint8_t value);
    bool pop(uint8_t& value);

private:
    std::array<uint8_t, BufferLength> buffer_{};
};
```

Avoid complex template metaprogramming unless there is a measured benefit.

---

## 18. Virtual Functions and Interfaces

### 18.1 Desktop Applications

Virtual interfaces are acceptable for polymorphism, plugins, GUI systems, simulation nodes, and test seams.

```cpp
class SimulationNode
{
public:
    virtual ~SimulationNode() = default;

    virtual void update(double deltaTimeSeconds) = 0;
};
```

### 18.2 Embedded Systems

Virtual functions may be used in embedded projects only when the cost is acceptable and the benefit is clear.

Acceptable reasons:

- hardware abstraction across board variants
- test interfaces
- simulation compatibility
- driver substitution

Avoid virtual dispatch in tight real-time loops unless measured.

---

## 19. Logging and Diagnostics

### 19.1 Desktop Applications

Desktop applications may use rich logging systems.

```cpp
logger.info("Loaded {} tag records from {}", tagCount, filePath.string());
```

### 19.2 Embedded Systems

Embedded logging should be controlled and lightweight.

Avoid expensive formatting in time-critical code.

Good:

```cpp
diagnostics.recordFault(FaultCode::MotorOverCurrent);
```

Potentially risky in firmware:

```cpp
printf("Motor current exceeded limit: %.3f A\n", measuredCurrentA);
```

Printing may be acceptable during development, but should not accidentally remain in hard real-time paths.

---

## 20. Testing

### 20.1 Prefer Testable Logic

Separate pure logic from hardware access when practical.

Good:

```cpp
float convertAdcCountToVoltage(uint16_t adcCount, float referenceVoltage);
```

This function can be tested without hardware.

### 20.2 Embedded Test Strategy

For embedded projects, prefer:

- pure function tests on desktop
- driver tests on target hardware
- hardware abstraction seams where useful
- simulation tests for control logic
- integration tests for timing-sensitive behaviour

### 20.3 Avoid Over-Abstracting Just for Tests

Do not damage the production code structure solely to make testing easier.

A test seam is useful only if it does not make the real system harder to understand.

---

## 21. Example: Desktop-Style C++

Desktop code may use modern C++ features freely.

```cpp
#include <filesystem>
#include <optional>
#include <string>
#include <vector>

struct TagRecord
{
    std::string name;
    std::string description;
    std::string dataType;
};

class TagDatabase
{
public:
    void loadFromFile(const std::filesystem::path& filePath);
    std::optional<TagRecord> findByName(const std::string& tagName) const;

private:
    std::vector<TagRecord> records_;
};
```

This is acceptable desktop-style C++.

It uses:

- `std::filesystem`
- `std::optional`
- `std::string`
- `std::vector`

These are appropriate for general software.

---

## 22. Example: Embedded-Style C++

Embedded code should be more explicit about memory, hardware, and state.

```cpp
#include <array>
#include <cstdint>

constexpr uint32_t kUartBaudRate = 115200;
constexpr size_t kTransmitBufferLength = 128;

enum class UartStatus
{
    Ok,
    NotInitialized,
    TransmitBufferFull
};

class UartPort
{
public:
    explicit UartPort(USART_TypeDef* instance);

    UartStatus initialize(uint32_t baudRate);
    UartStatus writeByte(uint8_t value);

private:
    USART_TypeDef* instance_ = nullptr;
    std::array<uint8_t, kTransmitBufferLength> transmitBuffer_{};
    bool isInitialized_ = false;
};
```

This is more appropriate for firmware because it:

- avoids heap allocation
- uses fixed-size buffers
- makes hardware ownership visible
- returns explicit status values
- keeps hardware state clear

---

## 23. Recommended Defaults

Unless a project states otherwise, use these defaults.

### General C++

- use the newest reliable C++ standard available
- minimum C++11
- use modern C++ features where they improve clarity
- use RAII where appropriate
- prefer standard library containers and algorithms
- use exceptions where appropriate for desktop applications
- use Doxygen for public APIs

### Embedded C++

- minimum C++11
- avoid exceptions unless explicitly allowed
- avoid RTTI unless explicitly allowed
- avoid heap allocation after initialization
- prefer fixed-size buffers
- prefer explicit error returns
- keep hardware side effects obvious
- avoid more than two abstraction layers for basic hardware behaviour
- use direct register access only in low-level driver layers
- document timing, memory, and hardware assumptions

### C

- minimum C99
- use fixed-width integer types for hardware-facing code
- use `snake_case` for C APIs
- use `extern "C"` for C-compatible headers used by C++
- avoid unnecessary macros
- keep modules small and focused

---

## 24. Final Rule

When unsure, choose the code that a tired engineer can read correctly under pressure.

Readable code is not just prettier. In engineering systems, readable code is safer, easier to debug, easier to verify, and less likely to fail in expensive ways.
