#include "EnvironmentCatalog.h"

namespace {
QList<EnvironmentOption> options(std::initializer_list<EnvironmentOption> values)
{
    return QList<EnvironmentOption>(values);
}
}

namespace EnvironmentCatalog {

QList<EnvironmentOption> ides()
{
    return options({
        {QStringLiteral("Visual Studio Code"), QStringLiteral("visual-studio-code")},
        {QStringLiteral("Visual Studio"), QStringLiteral("visual-studio")},
        {QStringLiteral("CLion"), QStringLiteral("clion")},
        {QStringLiteral("Qt Creator"), QStringLiteral("qt-creator")},
        {QStringLiteral("JetBrains Rider"), QStringLiteral("jetbrains-rider")},
        {QStringLiteral("IntelliJ IDEA"), QStringLiteral("intellij-idea")},
        {QStringLiteral("PyCharm"), QStringLiteral("pycharm")},
        {QStringLiteral("WebStorm"), QStringLiteral("webstorm")},
        {QStringLiteral("Eclipse"), QStringLiteral("eclipse")},
        {QStringLiteral("NetBeans"), QStringLiteral("netbeans")},
        {QStringLiteral("Xcode"), QStringLiteral("xcode")},
        {QStringLiteral("Android Studio"), QStringLiteral("android-studio")},
        {QStringLiteral("Arduino IDE"), QStringLiteral("arduino-ide")},
        {QStringLiteral("PlatformIO"), QStringLiteral("platformio")},
        {QStringLiteral("Vim"), QStringLiteral("vim")},
        {QStringLiteral("Neovim"), QStringLiteral("neovim")},
        {QStringLiteral("Emacs"), QStringLiteral("emacs")},
        {QStringLiteral("Sublime Text"), QStringLiteral("sublime-text")},
        {QStringLiteral("Notepad++"), QStringLiteral("notepad-plus-plus")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")},
        {QStringLiteral("None / Command Line Only"), QStringLiteral("none")}
    });
}

QList<EnvironmentOption> toolchains()
{
    return options({
        {QStringLiteral("MSYS2 UCRT64 GCC"), QStringLiteral("msys2-ucrt64-gcc")},
        {QStringLiteral("MSYS2 MINGW64 GCC"), QStringLiteral("msys2-mingw64-gcc")},
        {QStringLiteral("MinGW-w64 GCC"), QStringLiteral("mingw-w64-gcc")},
        {QStringLiteral("GCC"), QStringLiteral("gcc")},
        {QStringLiteral("Clang / LLVM"), QStringLiteral("clang-llvm")},
        {QStringLiteral("MSVC"), QStringLiteral("msvc")},
        {QStringLiteral("Apple Clang"), QStringLiteral("apple-clang")},
        {QStringLiteral("ARM GNU Toolchain"), QStringLiteral("arm-gnu")},
        {QStringLiteral("ARM Clang"), QStringLiteral("arm-clang")},
        {QStringLiteral("RISC-V GNU Toolchain"), QStringLiteral("riscv-gnu")},
        {QStringLiteral("ESP-IDF Toolchain"), QStringLiteral("esp-idf-toolchain")},
        {QStringLiteral("Pico SDK Toolchain"), QStringLiteral("pico-sdk-toolchain")},
        {QStringLiteral("AVR-GCC"), QStringLiteral("avr-gcc")},
        {QStringLiteral(".NET SDK"), QStringLiteral("dotnet-sdk")},
        {QStringLiteral("Java JDK"), QStringLiteral("java-jdk")},
        {QStringLiteral("Node.js"), QStringLiteral("nodejs")},
        {QStringLiteral("Python"), QStringLiteral("python")},
        {QStringLiteral("Rust Toolchain"), QStringLiteral("rust")},
        {QStringLiteral("Go Toolchain"), QStringLiteral("go")},
        {QStringLiteral("Swift Toolchain"), QStringLiteral("swift")},
        {QStringLiteral("Kotlin / JVM"), QStringLiteral("kotlin-jvm")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")},
        {QStringLiteral("Not Applicable"), QStringLiteral("not-applicable")}
    });
}

QList<EnvironmentOption> operatingSystems()
{
    return options({
        {QStringLiteral("Windows"), QStringLiteral("windows")},
        {QStringLiteral("Linux"), QStringLiteral("linux")},
        {QStringLiteral("macOS"), QStringLiteral("macos")},
        {QStringLiteral("FreeBSD"), QStringLiteral("freebsd")},
        {QStringLiteral("Other Unix"), QStringLiteral("other-unix")},
        {QStringLiteral("Cross-platform"), QStringLiteral("cross-platform")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> targets()
{
    return options({
        {QStringLiteral("Desktop"), QStringLiteral("desktop")},
        {QStringLiteral("Windows Desktop"), QStringLiteral("windows-desktop")},
        {QStringLiteral("Linux Desktop"), QStringLiteral("linux-desktop")},
        {QStringLiteral("macOS Desktop"), QStringLiteral("macos-desktop")},
        {QStringLiteral("Web Browser"), QStringLiteral("web-browser")},
        {QStringLiteral("Web Server"), QStringLiteral("web-server")},
        {QStringLiteral("Mobile"), QStringLiteral("mobile")},
        {QStringLiteral("Embedded System"), QStringLiteral("embedded-system")},
        {QStringLiteral("Microcontroller"), QStringLiteral("microcontroller")},
        {QStringLiteral("Bare Metal"), QStringLiteral("bare-metal")},
        {QStringLiteral("RTOS"), QStringLiteral("rtos")},
        {QStringLiteral("Single-Board Computer"), QStringLiteral("single-board-computer")},
        {QStringLiteral("Raspberry Pi / SBC"), QStringLiteral("raspberry-pi-sbc")},
        {QStringLiteral("Cloud"), QStringLiteral("cloud")},
        {QStringLiteral("Container"), QStringLiteral("container")},
        {QStringLiteral("Server"), QStringLiteral("server")},
        {QStringLiteral("Command Line"), QStringLiteral("command-line")},
        {QStringLiteral("Library"), QStringLiteral("library")},
        {QStringLiteral("Database / Data Processing"), QStringLiteral("database-data-processing")},
        {QStringLiteral("Game"), QStringLiteral("game")},
        {QStringLiteral("Simulation"), QStringLiteral("simulation")},
        {QStringLiteral("Scientific Computing"), QStringLiteral("scientific-computing")},
        {QStringLiteral("AI / Machine Learning"), QStringLiteral("ai-machine-learning")},
        {QStringLiteral("Android"), QStringLiteral("android")},
        {QStringLiteral("iOS"), QStringLiteral("ios")},
        {QStringLiteral("Browser Extension"), QStringLiteral("browser-extension")},
        {QStringLiteral("Desktop Service / Daemon"), QStringLiteral("desktop-service")},
        {QStringLiteral("Cross-platform"), QStringLiteral("cross-platform")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> architectures()
{
    return options({
        {QStringLiteral("Auto / Template Default"), QStringLiteral("auto")},
        {QStringLiteral("x86"), QStringLiteral("x86")},
        {QStringLiteral("x86_64"), QStringLiteral("x86_64")},
        {QStringLiteral("ARM32"), QStringLiteral("arm32")},
        {QStringLiteral("ARM64"), QStringLiteral("arm64")},
        {QStringLiteral("Cortex-M"), QStringLiteral("cortex-m")},
        {QStringLiteral("Cortex-A"), QStringLiteral("cortex-a")},
        {QStringLiteral("RISC-V"), QStringLiteral("risc-v")},
        {QStringLiteral("Xtensa"), QStringLiteral("xtensa")},
        {QStringLiteral("AVR"), QStringLiteral("avr")},
        {QStringLiteral("PowerPC"), QStringLiteral("powerpc")},
        {QStringLiteral("MIPS"), QStringLiteral("mips")},
        {QStringLiteral("RP2040"), QStringLiteral("rp2040")},
        {QStringLiteral("RP2350"), QStringLiteral("rp2350")},
        {QStringLiteral("WebAssembly"), QStringLiteral("webassembly")},
        {QStringLiteral("Multi-architecture"), QStringLiteral("multi-architecture")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")},
        {QStringLiteral("Not Applicable"), QStringLiteral("not-applicable")}
    });
}

QList<EnvironmentOption> buildSystems()
{
    return options({
        {QStringLiteral("CMake"), QStringLiteral("cmake")},
        {QStringLiteral("Ninja"), QStringLiteral("ninja")},
        {QStringLiteral("Make"), QStringLiteral("make")},
        {QStringLiteral("MSBuild"), QStringLiteral("msbuild")},
        {QStringLiteral("Visual Studio Build System"), QStringLiteral("visual-studio-build")},
        {QStringLiteral("Meson"), QStringLiteral("meson")},
        {QStringLiteral("Bazel"), QStringLiteral("bazel")},
        {QStringLiteral("Premake"), QStringLiteral("premake")},
        {QStringLiteral("QMake"), QStringLiteral("qmake")},
        {QStringLiteral("Gradle"), QStringLiteral("gradle")},
        {QStringLiteral("Maven"), QStringLiteral("maven")},
        {QStringLiteral("npm"), QStringLiteral("npm")},
        {QStringLiteral("pnpm"), QStringLiteral("pnpm")},
        {QStringLiteral("Yarn"), QStringLiteral("yarn")},
        {QStringLiteral("Cargo"), QStringLiteral("cargo")},
        {QStringLiteral("Go Build"), QStringLiteral("go-build")},
        {QStringLiteral("Python setuptools"), QStringLiteral("python-setuptools")},
        {QStringLiteral("Python Poetry"), QStringLiteral("python-poetry")},
        {QStringLiteral("PlatformIO"), QStringLiteral("platformio")},
        {QStringLiteral("Arduino Build System"), QStringLiteral("arduino-build")},
        {QStringLiteral("ESP-IDF"), QStringLiteral("esp-idf")},
        {QStringLiteral("Pico SDK / CMake"), QStringLiteral("pico-sdk-cmake")},
        {QStringLiteral("Xcode Build System"), QStringLiteral("xcode-build")},
        {QStringLiteral("Custom Script"), QStringLiteral("custom-script")},
        {QStringLiteral("None"), QStringLiteral("none")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> languages()
{
    return options({
        {QStringLiteral("C++"), QStringLiteral("cpp")},
        {QStringLiteral("C"), QStringLiteral("c")},
        {QStringLiteral("Java"), QStringLiteral("java")},
        {QStringLiteral("Kotlin"), QStringLiteral("kotlin")},
        {QStringLiteral("JavaScript"), QStringLiteral("javascript")},
        {QStringLiteral("Python"), QStringLiteral("python")},
        {QStringLiteral("TypeScript"), QStringLiteral("typescript")},
        {QStringLiteral("C#"), QStringLiteral("csharp")},
        {QStringLiteral("Rust"), QStringLiteral("rust")},
        {QStringLiteral("Go"), QStringLiteral("go")},
        {QStringLiteral("Swift"), QStringLiteral("swift")},
        {QStringLiteral("SQL"), QStringLiteral("sql")},
        {QStringLiteral("HTML"), QStringLiteral("html")},
        {QStringLiteral("CSS"), QStringLiteral("css")},
        {QStringLiteral("Assembly"), QStringLiteral("assembly")},
        {QStringLiteral("PIO Assembly"), QStringLiteral("pio-assembly")},
        {QStringLiteral("Bash / Shell"), QStringLiteral("bash")},
        {QStringLiteral("PowerShell"), QStringLiteral("powershell")},
        {QStringLiteral("Zig"), QStringLiteral("zig")},
        {QStringLiteral("Dart"), QStringLiteral("dart")},
        {QStringLiteral("PHP"), QStringLiteral("php")},
        {QStringLiteral("Ruby"), QStringLiteral("ruby")},
        {QStringLiteral("Lua"), QStringLiteral("lua")},
        {QStringLiteral("Objective-C"), QStringLiteral("objective-c")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> frameworks()
{
    return options({
        {QStringLiteral("Qt"), QStringLiteral("qt")},
        {QStringLiteral("wxWidgets"), QStringLiteral("wxwidgets")},
        {QStringLiteral("GTK"), QStringLiteral("gtk")},
        {QStringLiteral(".NET"), QStringLiteral("dotnet")},
        {QStringLiteral("WinUI"), QStringLiteral("winui")},
        {QStringLiteral("SDL"), QStringLiteral("sdl")},
        {QStringLiteral("SFML"), QStringLiteral("sfml")},
        {QStringLiteral("React"), QStringLiteral("react")},
        {QStringLiteral("Vue"), QStringLiteral("vue")},
        {QStringLiteral("Angular"), QStringLiteral("angular")},
        {QStringLiteral("Svelte"), QStringLiteral("svelte")},
        {QStringLiteral("Express"), QStringLiteral("express")},
        {QStringLiteral("Node.js"), QStringLiteral("nodejs")},
        {QStringLiteral("Django"), QStringLiteral("django")},
        {QStringLiteral("Flask"), QStringLiteral("flask")},
        {QStringLiteral("FastAPI"), QStringLiteral("fastapi")},
        {QStringLiteral("Next.js"), QStringLiteral("nextjs")},
        {QStringLiteral("Nuxt"), QStringLiteral("nuxt")},
        {QStringLiteral("NestJS"), QStringLiteral("nestjs")},
        {QStringLiteral("ASP.NET Core"), QStringLiteral("aspnet")},
        {QStringLiteral("Raspberry Pi Pico SDK"), QStringLiteral("pico-sdk")},
        {QStringLiteral("Arduino"), QStringLiteral("arduino")},
        {QStringLiteral("PlatformIO"), QStringLiteral("platformio")},
        {QStringLiteral("ESP-IDF"), QStringLiteral("esp-idf")},
        {QStringLiteral("Zephyr"), QStringLiteral("zephyr")},
        {QStringLiteral("FreeRTOS"), QStringLiteral("freertos")},
        {QStringLiteral("Flutter"), QStringLiteral("flutter")},
        {QStringLiteral("React Native"), QStringLiteral("react-native")},
        {QStringLiteral(".NET MAUI"), QStringLiteral("dotnet-maui")},
        {QStringLiteral("Android SDK"), QStringLiteral("android-sdk")},
        {QStringLiteral("Android Emulator"), QStringLiteral("android-emulator")},
        {QStringLiteral("Android Device Testing"), QStringLiteral("android-device-testing")},
        {QStringLiteral("Jetpack Compose"), QStringLiteral("jetpack-compose")},
        {QStringLiteral("Room"), QStringLiteral("room")},
        {QStringLiteral("STM32 HAL / Cube"), QStringLiteral("stm32-hal")},
        {QStringLiteral("CMSIS"), QStringLiteral("cmsis")},
        {QStringLiteral("TinyUSB"), QStringLiteral("tinyusb")},
        {QStringLiteral("Unreal Engine"), QStringLiteral("unreal")},
        {QStringLiteral("Unity"), QStringLiteral("unity")},
        {QStringLiteral("Godot"), QStringLiteral("godot")},
        {QStringLiteral("OpenGL"), QStringLiteral("opengl")},
        {QStringLiteral("Vulkan"), QStringLiteral("vulkan")},
        {QStringLiteral("PyTorch"), QStringLiteral("pytorch")},
        {QStringLiteral("TensorFlow"), QStringLiteral("tensorflow")},
        {QStringLiteral("Custom Framework / SDK"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> hardwareTargets()
{
    return options({
        {QStringLiteral("Desktop PC"), QStringLiteral("desktop-pc")}, {QStringLiteral("Laptop"), QStringLiteral("laptop")}, {QStringLiteral("Server"), QStringLiteral("server")}, {QStringLiteral("Virtual Machine"), QStringLiteral("virtual-machine")}, {QStringLiteral("Docker / Container Runtime"), QStringLiteral("docker")}, {QStringLiteral("Raspberry Pi"), QStringLiteral("raspberry-pi")}, {QStringLiteral("Raspberry Pi Pico"), QStringLiteral("raspberry-pi-pico")}, {QStringLiteral("Raspberry Pi Pico W"), QStringLiteral("raspberry-pi-pico-w")}, {QStringLiteral("Raspberry Pi Pico 2"), QStringLiteral("raspberry-pi-pico-2")}, {QStringLiteral("Raspberry Pi Pico 2 W"), QStringLiteral("raspberry-pi-pico-2-w")}, {QStringLiteral("ESP32"), QStringLiteral("esp32")}, {QStringLiteral("Arduino-compatible MCU"), QStringLiteral("arduino-mcu")}, {QStringLiteral("Other Microcontroller"), QStringLiteral("other-microcontroller")}, {QStringLiteral("Mobile Device"), QStringLiteral("mobile-device")}, {QStringLiteral("Cloud Environment"), QStringLiteral("cloud-environment")}, {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> buildConfigurations()
{
    return options({{QStringLiteral("Debug"), QStringLiteral("debug")}, {QStringLiteral("Release"), QStringLiteral("release")}, {QStringLiteral("RelWithDebInfo"), QStringLiteral("rel-with-deb-info")}, {QStringLiteral("MinSizeRel"), QStringLiteral("min-size-rel")}, {QStringLiteral("Custom"), QStringLiteral("custom")} });
}

QList<EnvironmentOption> toolingCapabilities()
{
    return options({{QStringLiteral("Unit Testing"), QStringLiteral("unit-testing")}, {QStringLiteral("Integration Testing"), QStringLiteral("integration-testing")}, {QStringLiteral("End-to-End Testing"), QStringLiteral("e2e-testing")}, {QStringLiteral("Hardware-in-the-loop Testing"), QStringLiteral("hardware-in-loop")}, {QStringLiteral("Static Analysis"), QStringLiteral("static-analysis")}, {QStringLiteral("Code Formatting"), QStringLiteral("formatting")}, {QStringLiteral("Linting"), QStringLiteral("linting")}, {QStringLiteral("Sanitizers"), QStringLiteral("sanitizers")}, {QStringLiteral("Coverage"), QStringLiteral("coverage")}, {QStringLiteral("Profiling"), QStringLiteral("profiling")}, {QStringLiteral("Debugger"), QStringLiteral("debugger")}, {QStringLiteral("Remote Debugging"), QStringLiteral("remote-debugging")}, {QStringLiteral("Hardware Debug Probe"), QStringLiteral("hardware-debug-probe")}, {QStringLiteral("CI"), QStringLiteral("ci")}, {QStringLiteral("CD"), QStringLiteral("cd")}, {QStringLiteral("Automated Build"), QStringLiteral("automated-build")}, {QStringLiteral("Automated Testing"), QStringLiteral("automated-testing")}, {QStringLiteral("Package / Installer"), QStringLiteral("package-installer")}, {QStringLiteral("Container Image"), QStringLiteral("container-image")}, {QStringLiteral("Firmware Image"), QStringLiteral("firmware-image")}, {QStringLiteral("Release Archive"), QStringLiteral("release-archive")} });
}

QList<EnvironmentOption> versionControlSystems()
{
    return options({{"Git", "git"}, {"Git LFS", "git-lfs"}, {"Subversion", "subversion"}, {"Mercurial", "mercurial"}, {"None", "none"}, {"Other / Custom", "custom"}});
}

QList<EnvironmentOption> developmentSupport()
{
    return options({{"Debugger", "debugger"}, {"Remote Debugging", "remote-debugging"}, {"Hardware Debug Probe", "hardware-debug-probe"}, {"Profiling", "profiling"}, {"Memory Analysis", "memory-analysis"}});
}

QList<EnvironmentOption> processorFamilies()
{
    return options({{"RP2040", "rp2040"}, {"RP2350", "rp2350"}, {"ESP32", "esp32"}, {"STM32", "stm32"}, {"nRF52", "nrf52"}, {"nRF53", "nrf53"}, {"AVR", "avr"}, {"Other / Custom", "custom"}});
}

QList<EnvironmentOption> dependencyManagers()
{
    return options({{"vcpkg", "vcpkg"}, {"Conan", "conan"}, {"CPM.cmake", "cpm-cmake"}, {"CMake FetchContent", "cmake-fetchcontent"}, {"NuGet", "nuget"}, {"npm", "npm"}, {"pnpm", "pnpm"}, {"Yarn", "yarn"}, {"pip", "pip"}, {"Poetry", "poetry"}, {"Cargo", "cargo"}, {"Maven", "maven"}, {"Gradle", "gradle"}, {"Go Modules", "go-modules"}, {"Other / Custom", "custom"}, {"None", "none"}});
}

QList<EnvironmentOption> testingCapabilities()
{
    return options({{"Unit Testing", "unit-testing"}, {"Integration Testing", "integration-testing"}, {"End-to-End Testing", "e2e-testing"}, {"Hardware-in-the-loop Testing", "hardware-in-loop"}, {"Fuzz Testing", "fuzz-testing"}, {"Benchmarking", "benchmarking"}});
}

QList<EnvironmentOption> qualityCapabilities()
{
    return options({{"Static Analysis", "static-analysis"}, {"Linting", "linting"}, {"Code Formatting", "formatting"}, {"Sanitizers", "sanitizers"}, {"Coverage", "coverage"}, {"Profiling", "profiling"}, {"Memory Analysis", "memory-analysis"}, {"Security Scanning", "security-scanning"}, {"Dependency Scanning", "dependency-scanning"}});
}

QList<EnvironmentOption> automationCapabilities()
{
    return options({{"CI", "ci"}, {"CD", "cd"}, {"Automated Build", "automated-build"}, {"Automated Testing", "automated-testing"}, {"Code Generation", "code-generation"}, {"Documentation Generation", "documentation-generation"}});
}

QList<EnvironmentOption> deliveryCapabilities()
{
    return options({{"Package / Installer", "package-installer"}, {"Container Image", "container-image"}, {"Firmware Image", "firmware-image"}, {"Release Archive", "release-archive"}, {"Signing", "signing"}, {"SBOM Generation", "sbom-generation"}});
}

QList<EnvironmentOption> academicModes()
{
    return options({
        {QStringLiteral("Disabled"), QStringLiteral("disabled")},
        {QStringLiteral("Academic Assignment"), QStringLiteral("academic-assignment")},
        {QStringLiteral("Research Project"), QStringLiteral("research-project")},
        {QStringLiteral("Thesis"), QStringLiteral("thesis")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> thesisLevels()
{
    return options({
        {QStringLiteral("Bachelor"), QStringLiteral("bachelor")},
        {QStringLiteral("Master"), QStringLiteral("master")},
        {QStringLiteral("Doctoral"), QStringLiteral("doctoral")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> thesisApproaches()
{
    return options({
        {QStringLiteral("Software / System Development"), QStringLiteral("software-system-development")},
        {QStringLiteral("Experimental / Empirical"), QStringLiteral("experimental-empirical")},
        {QStringLiteral("Theoretical"), QStringLiteral("theoretical")},
        {QStringLiteral("Literature Review"), QStringLiteral("literature-review")},
        {QStringLiteral("Design Science"), QStringLiteral("design-science")},
        {QStringLiteral("Case Study"), QStringLiteral("case-study")},
        {QStringLiteral("Data Analysis"), QStringLiteral("data-analysis")},
        {QStringLiteral("Simulation / Modelling"), QStringLiteral("simulation-modelling")},
        {QStringLiteral("Hardware / Embedded Development"), QStringLiteral("hardware-embedded-development")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> researchMethods()
{
    return options({
        {QStringLiteral("Qualitative"), QStringLiteral("qualitative")},
        {QStringLiteral("Quantitative"), QStringLiteral("quantitative")},
        {QStringLiteral("Mixed Methods"), QStringLiteral("mixed-methods")},
        {QStringLiteral("Experiment"), QStringLiteral("experiment")},
        {QStringLiteral("Case Study"), QStringLiteral("case-study")},
        {QStringLiteral("Survey"), QStringLiteral("survey")},
        {QStringLiteral("Interviews"), QStringLiteral("interviews")},
        {QStringLiteral("Observation"), QStringLiteral("observation")},
        {QStringLiteral("Literature Review"), QStringLiteral("literature-review")},
        {QStringLiteral("Systematic Literature Review"), QStringLiteral("systematic-literature-review")},
        {QStringLiteral("Prototype Evaluation"), QStringLiteral("prototype-evaluation")},
        {QStringLiteral("Benchmarking"), QStringLiteral("benchmarking")},
        {QStringLiteral("Simulation"), QStringLiteral("simulation")},
        {QStringLiteral("Statistical Analysis"), QStringLiteral("statistical-analysis")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> citationStyles()
{
    return options({
        {QStringLiteral("None / Not Selected"), QStringLiteral("none")},
        {QStringLiteral("APA"), QStringLiteral("apa")},
        {QStringLiteral("IEEE"), QStringLiteral("ieee")},
        {QStringLiteral("Harvard"), QStringLiteral("harvard")},
        {QStringLiteral("Chicago"), QStringLiteral("chicago")},
        {QStringLiteral("Vancouver"), QStringLiteral("vancouver")},
        {QStringLiteral("MLA"), QStringLiteral("mla")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> academicLanguages()
{
    return options({
        {QStringLiteral("English"), QStringLiteral("english")},
        {QStringLiteral("Swedish"), QStringLiteral("swedish")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> academicRequirements()
{
    return options({
        {QStringLiteral("Source Citations"), QStringLiteral("source-citations")},
        {QStringLiteral("Reference List"), QStringLiteral("reference-list")},
        {QStringLiteral("Methodology Section"), QStringLiteral("methodology-section")},
        {QStringLiteral("Related Work / Background"), QStringLiteral("related-work-background")},
        {QStringLiteral("Research Questions"), QStringLiteral("research-questions")},
        {QStringLiteral("Hypothesis"), QStringLiteral("hypothesis")},
        {QStringLiteral("Ethics Consideration"), QStringLiteral("ethics-consideration")},
        {QStringLiteral("Reproducibility"), QStringLiteral("reproducibility")},
        {QStringLiteral("Data Management"), QStringLiteral("data-management")},
        {QStringLiteral("Appendices"), QStringLiteral("appendices")},
        {QStringLiteral("Academic Formatting"), QStringLiteral("academic-formatting")},
        {QStringLiteral("Plagiarism / Originality Check"), QStringLiteral("originality-check")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> academicDeliverables()
{
    return options({
        {QStringLiteral("Written Thesis"), QStringLiteral("written-thesis")},
        {QStringLiteral("Academic Report"), QStringLiteral("academic-report")},
        {QStringLiteral("Source Code"), QStringLiteral("source-code")},
        {QStringLiteral("Prototype"), QStringLiteral("prototype")},
        {QStringLiteral("Dataset"), QStringLiteral("dataset")},
        {QStringLiteral("Presentation"), QStringLiteral("presentation")},
        {QStringLiteral("Poster"), QStringLiteral("poster")},
        {QStringLiteral("Demonstration"), QStringLiteral("demonstration")},
        {QStringLiteral("Research Log"), QStringLiteral("research-log")},
        {QStringLiteral("Appendices"), QStringLiteral("appendices")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> resourceTypes()
{
    return options({
        {QStringLiteral("File"), QStringLiteral("file")}, {QStringLiteral("Folder"), QStringLiteral("folder")},
        {QStringLiteral("URL"), QStringLiteral("url")}, {QStringLiteral("Datasheet"), QStringLiteral("datasheet")},
        {QStringLiteral("Specification"), QStringLiteral("specification")}, {QStringLiteral("SDK Documentation"), QStringLiteral("sdk-documentation")},
        {QStringLiteral("API Documentation"), QStringLiteral("api-documentation")}, {QStringLiteral("Source Code"), QStringLiteral("source-code")},
        {QStringLiteral("Reference Implementation"), QStringLiteral("reference-implementation")}, {QStringLiteral("Database"), QStringLiteral("database")},
        {QStringLiteral("Research Paper"), QStringLiteral("research-paper")}, {QStringLiteral("Standard"), QStringLiteral("standard")},
        {QStringLiteral("Test Report"), QStringLiteral("test-report")}, {QStringLiteral("Diagram"), QStringLiteral("diagram")},
        {QStringLiteral("Schematic"), QStringLiteral("schematic")}, {QStringLiteral("CAD / Model"), QStringLiteral("cad-model")},
        {QStringLiteral("Image"), QStringLiteral("image")}, {QStringLiteral("Project Document"), QStringLiteral("project-document")},
        {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> authorityLevels()
{
    return options({
        {QStringLiteral("Primary Source of Truth"), QStringLiteral("primary-source-of-truth")},
        {QStringLiteral("Authoritative"), QStringLiteral("authoritative")},
        {QStringLiteral("Trusted Reference"), QStringLiteral("trusted-reference")},
        {QStringLiteral("Supporting Reference"), QStringLiteral("supporting-reference")},
        {QStringLiteral("Historical / Legacy"), QStringLiteral("historical")},
        {QStringLiteral("Informational"), QStringLiteral("informational")},
        {QStringLiteral("Untrusted / Do Not Rely On"), QStringLiteral("untrusted")}
    });
}

QList<EnvironmentOption> resourceScopes()
{
    return options({
        {QStringLiteral("Entire Project"), QStringLiteral("entire-project")}, {QStringLiteral("Project Requirements"), QStringLiteral("project-requirements")},
        {QStringLiteral("Architecture"), QStringLiteral("architecture")}, {QStringLiteral("Software"), QStringLiteral("software")},
        {QStringLiteral("Hardware"), QStringLiteral("hardware")}, {QStringLiteral("Firmware"), QStringLiteral("firmware")},
        {QStringLiteral("SDK / API"), QStringLiteral("sdk-api")}, {QStringLiteral("Build System"), QStringLiteral("build-system")},
        {QStringLiteral("Testing"), QStringLiteral("testing")}, {QStringLiteral("Security"), QStringLiteral("security")},
        {QStringLiteral("UI / UX"), QStringLiteral("ui-ux")}, {QStringLiteral("Database / Data"), QStringLiteral("database-data")},
        {QStringLiteral("Academic"), QStringLiteral("academic")}, {QStringLiteral("Deployment"), QStringLiteral("deployment")},
        {QStringLiteral("Current Project State"), QStringLiteral("current-project-state")}, {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
    });
}

QList<EnvironmentOption> resourcePolicyOptions()
{
    return options({
        {QStringLiteral("Read resources when relevant"), QStringLiteral("read-relevant")},
        {QStringLiteral("Prefer authoritative sources"), QStringLiteral("prefer-authoritative")},
        {QStringLiteral("Respect resource scope"), QStringLiteral("respect-scope")},
        {QStringLiteral("Ignore disabled resources"), QStringLiteral("ignore-disabled")},
        {QStringLiteral("Warn when sources conflict"), QStringLiteral("warn-conflicts")},
        {QStringLiteral("Cite sources for technical claims"), QStringLiteral("cite-technical-claims")},
        {QStringLiteral("Record source used for major decisions"), QStringLiteral("record-major-decision-source")},
        {QStringLiteral("Prefer project-local resources"), QStringLiteral("prefer-project-local")},
        {QStringLiteral("Allow external URL references"), QStringLiteral("allow-external-urls")},
        {QStringLiteral("Allow generated resource summaries"), QStringLiteral("allow-summaries")},
        {QStringLiteral("Allow AI to infer missing information"), QStringLiteral("infer-missing")},
        {QStringLiteral("Allow external web fallback"), QStringLiteral("external-web-fallback")},
        {QStringLiteral("Require authority before changing conflicting information"), QStringLiteral("require-authority-before-change")}
    });
}

QList<EnvironmentOption> resourceLoadingStrategies()
{
    return options({
        {QStringLiteral("Load when relevant"), QStringLiteral("relevant")},
        {QStringLiteral("Always load"), QStringLiteral("always")},
        {QStringLiteral("Load only when explicitly requested"), QStringLiteral("explicit")},
        {QStringLiteral("Metadata only until needed"), QStringLiteral("metadata-only")}
    });
}

}
