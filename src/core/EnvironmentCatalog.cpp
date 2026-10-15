#include "EnvironmentCatalog.h"
#include <QJsonDocument>

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
        {QStringLiteral("Python venv"), QStringLiteral("python-venv")},
        {QStringLiteral("Conda"), QStringLiteral("conda")},
        {QStringLiteral("Miniconda"), QStringLiteral("miniconda")},
        {QStringLiteral("uv"), QStringLiteral("uv")},
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
    return options({{"AndroidX ViewModel", "androidx-viewmodel"}, {"Kotlin coroutines / StateFlow", "kotlin-coroutines"}, {"Retrofit", "retrofit"},
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
        {QStringLiteral("scikit-learn"), QStringLiteral("scikit-learn")},
        {QStringLiteral("Keras"), QStringLiteral("keras")},
        {QStringLiteral("XGBoost"), QStringLiteral("xgboost")},
        {QStringLiteral("LightGBM"), QStringLiteral("lightgbm")},
        {QStringLiteral("Hugging Face Transformers"), QStringLiteral("hugging-face-transformers")},
        {QStringLiteral("NumPy"), QStringLiteral("numpy")},
        {QStringLiteral("pandas"), QStringLiteral("pandas")},
        {QStringLiteral("SciPy"), QStringLiteral("scipy")},
        {QStringLiteral("matplotlib"), QStringLiteral("matplotlib")},
        {QStringLiteral("OpenCV"), QStringLiteral("opencv")},
        {QStringLiteral("ONNX"), QStringLiteral("onnx")},
        {QStringLiteral("ONNX Runtime"), QStringLiteral("onnx-runtime")},
        {QStringLiteral("TensorFlow Lite"), QStringLiteral("tensorflow-lite")},
        {QStringLiteral("CUDA"), QStringLiteral("cuda")},
        {QStringLiteral("cuDNN"), QStringLiteral("cudnn")},
        {QStringLiteral("Custom Framework / SDK"), QStringLiteral("custom")},
        {QStringLiteral("Retrofit"), QStringLiteral("custom:Retrofit")}
    });
}

namespace {
QList<EnvironmentOption> selectedOptions(const QList<EnvironmentOption>& source, const QStringList& ids)
{
    QList<EnvironmentOption> result;
    for (const auto& option : source) if (ids.contains(option.second)) result.append(option);
    return result;
}
}

QList<EnvironmentOption> languageSystems()
// Brief: Return the non-overlapping systems language catalog.
// Keep the catalog source authoritative and filter by stable capability IDs to preserve selection identity.
{ return selectedOptions(languages(), {"cpp", "c", "java", "kotlin", "csharp", "rust", "go", "swift", "assembly", "pio-assembly", "zig", "objective-c"}); }

QList<EnvironmentOption> languageApplication()
// Brief: Return application and data languages.
// This group owns scripting and query languages plus the custom-entry selector.
{ return selectedOptions(languages(), {"python", "sql", "dart", "php", "custom"}); }

QList<EnvironmentOption> languageWeb()
// Brief: Return web document and application languages.
// IDs are drawn from the canonical language catalog and appear in this group only.
{ return selectedOptions(languages(), {"javascript", "typescript", "html", "css"}); }

QList<EnvironmentOption> languageScripting()
// Brief: Return shell and automation languages.
// Keep this optional group empty when the canonical catalog has no separate scripting-only entries.
{ return selectedOptions(languages(), {"bash", "powershell", "ruby", "lua"}); }

QList<EnvironmentOption> frameworkDesktopUi()
// Brief: Return desktop and UI frameworks.
// The custom framework selector stays accessible from this broad UI category.
{ return selectedOptions(frameworks(), {"qt", "wxwidgets", "gtk", "dotnet", "winui", "custom"}); }

QList<EnvironmentOption> frameworkWebServer()
// Brief: Return web and server frameworks.
// Keep the existing custom Retrofit entry intact alongside the canonical catalog IDs.
{ return selectedOptions(frameworks(), {"react", "vue", "angular", "svelte", "express", "nodejs", "django", "flask", "fastapi", "nextjs", "nuxt", "nestjs", "aspnet", "custom:Retrofit"}); }

QList<EnvironmentOption> frameworkMobile()
// Brief: Return mobile frameworks and SDKs.
// Android SDK, emulators, testing libraries, and UI libraries remain together in this domain.
{ return selectedOptions(frameworks(), {"flutter", "react-native", "dotnet-maui", "android-sdk", "android-emulator", "android-device-testing", "jetpack-compose", "room", "androidx-viewmodel", "kotlin-coroutines", "retrofit"}); }

QList<EnvironmentOption> frameworkEmbeddedDevice()
// Brief: Return embedded and device frameworks.
// Preserve all current microcontroller SDK and RTOS selections.
{ return selectedOptions(frameworks(), {"pico-sdk", "arduino", "platformio", "esp-idf", "zephyr", "freertos", "stm32-hal", "cmsis", "tinyusb"}); }

QList<EnvironmentOption> frameworkDataMl()
// Brief: Return data processing and machine learning libraries.
// Catalog membership remains stable, including accelerator and model-runtime options.
{ return selectedOptions(frameworks(), {"pytorch", "tensorflow", "scikit-learn", "keras", "xgboost", "lightgbm", "hugging-face-transformers", "numpy", "pandas", "scipy", "matplotlib", "opencv", "onnx", "onnx-runtime", "tensorflow-lite", "cuda", "cudnn"}); }

QList<EnvironmentOption> frameworkGraphicsGame()
// Brief: Return graphics and game development frameworks.
// These entries are separate from application UI and data/ML libraries.
{ return selectedOptions(frameworks(), {"sdl", "sfml", "unreal", "unity", "godot", "opengl", "vulkan"}); }

QList<EnvironmentOption> hardwareTargets()
{
    return options({
        {QStringLiteral("CPU"), QStringLiteral("cpu")}, {QStringLiteral("NVIDIA GPU"), QStringLiteral("nvidia-gpu")}, {QStringLiteral("Desktop PC"), QStringLiteral("desktop-pc")}, {QStringLiteral("Laptop"), QStringLiteral("laptop")}, {QStringLiteral("Server"), QStringLiteral("server")}, {QStringLiteral("Virtual Machine"), QStringLiteral("virtual-machine")}, {QStringLiteral("Docker / Container Runtime"), QStringLiteral("docker")}, {QStringLiteral("Raspberry Pi"), QStringLiteral("raspberry-pi")}, {QStringLiteral("Raspberry Pi Pico"), QStringLiteral("raspberry-pi-pico")}, {QStringLiteral("Raspberry Pi Pico W"), QStringLiteral("raspberry-pi-pico-w")}, {QStringLiteral("Raspberry Pi Pico 2"), QStringLiteral("raspberry-pi-pico-2")}, {QStringLiteral("Raspberry Pi Pico 2 W"), QStringLiteral("raspberry-pi-pico-2-w")}, {QStringLiteral("ESP32"), QStringLiteral("esp32")}, {QStringLiteral("Arduino-compatible MCU"), QStringLiteral("arduino-mcu")}, {QStringLiteral("Other Microcontroller"), QStringLiteral("other-microcontroller")}, {QStringLiteral("Mobile Device"), QStringLiteral("mobile-device")}, {QStringLiteral("Cloud Environment"), QStringLiteral("cloud-environment")}, {QStringLiteral("Other / Custom"), QStringLiteral("custom")}
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
    return options({{"Android platform tools / adb", "android-platform-tools"}, {"Android build tools", "android-build-tools"}, {"Android Emulator / AVD", "android-emulator"}, {"Gradle Wrapper", "gradle-wrapper"}, {"GitHub", "github"}, {"Debugger", "debugger"}, {"Remote Debugging", "remote-debugging"}, {"Hardware Debug Probe", "hardware-debug-probe"}, {"Profiling", "profiling"}, {"Memory Analysis", "memory-analysis"}, {"Android SDK", "android-sdk"}, {"Gradle", "gradle"}, {"Android Studio", "android-studio"}, {"Java JDK", "java-jdk"}, {"Arduino IDE", "arduino-ide"}, {"Jupyter Notebook", "jupyter-notebook"}, {"JupyterLab", "jupyterlab"}});
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
    return options({{"JUnit", "junit"}, {"Android instrumentation / Compose UI tests", "android-instrumentation"}, {"Android connected device tests", "android-device-testing"}, {"Unit Testing", "unit-testing"}, {"Integration Testing", "integration-testing"}, {"End-to-End Testing", "e2e-testing"}, {"Hardware-in-the-loop Testing", "hardware-in-loop"}, {"Fuzz Testing", "fuzz-testing"}, {"Benchmarking", "benchmarking"}});
}

QList<EnvironmentOption> qualityCapabilities()
{
    return options({{"Static Analysis", "static-analysis"}, {"Linting", "linting"}, {"Code Formatting", "formatting"}, {"Sanitizers", "sanitizers"}, {"Coverage", "coverage"}, {"Profiling", "profiling"}, {"Memory Analysis", "memory-analysis"}, {"Security Scanning", "security-scanning"}, {"Dependency Scanning", "dependency-scanning"}});
}

QList<EnvironmentOption> automationCapabilities()
{
    return options({{"GitHub Actions", "github-actions"}, {"CI", "ci"}, {"CD", "cd"}, {"Automated Build", "automated-build"}, {"Automated Testing", "automated-testing"}, {"Code Generation", "code-generation"}, {"Documentation Generation", "documentation-generation"}});
}

QList<EnvironmentOption> deliveryCapabilities()
{
    return options({{"Package / Installer", "package-installer"}, {"Container Image", "container-image"}, {"Firmware Image", "firmware-image"}, {"Release Archive", "release-archive"}, {"Signing", "signing"}, {"SBOM Generation", "sbom-generation"}});
}

QList<EnvironmentOption> academicModes()
{
    return options({
        {QStringLiteral("Academic Assignment"), QStringLiteral("academic-assignment")},
        {QStringLiteral("Research Project"), QStringLiteral("research-project")},
        {QStringLiteral("Thesis"), QStringLiteral("thesis-project")},
        {QStringLiteral("Report"), QStringLiteral("report-project")},
        {QStringLiteral("Other / Custom"), QStringLiteral("other-custom")}
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

QList<EnvironmentOption> governanceRoles()
{
    return options({
        {QStringLiteral("Source of Truth"), QStringLiteral("source-of-truth")},
        {QStringLiteral("Instruction"), QStringLiteral("instruction")},
        {QStringLiteral("Reference"), QStringLiteral("reference")},
        {QStringLiteral("Supporting Material"), QStringLiteral("supporting-material")}
        ,{QStringLiteral("Thesis Template"), QStringLiteral("thesis-template")}
        ,{QStringLiteral("Report Template"), QStringLiteral("report-template")}
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

QJsonObject EnvironmentCatalog::projectProfile(const QString& id)
// Brief: Return source-qualified, independent project defaults without performing external actions.
{
    static const auto profiles = QJsonDocument::fromJson(QByteArray(R"ARAMF({
  "storepath-android": {
    "name": "StorePath Android",
    "source": "https://github.com/morganlindbom/storePath/tree/b9489f02ec2c4cc6e5bdbc28adf593e8dc1cb4b6",
    "settings": {
      "minSdk": "24",
      "targetSdk": "36",
      "compileSdk": "37",
      "javaTarget": "11",
      "kotlinJvmTarget": "11 (derived from Java target; not explicitly pinned)",
      "androidGradlePlugin": "9.2.1",
      "kotlinComposePlugin": "2.2.10",
      "kotlinCompiler": "AGP-managed built-in Kotlin",
      "gradleWrapper": "9.4.1",
      "gradleRuntimeJdk": "21",
      "ciBootstrapJdk": "Temurin 17",
      "sdkBuildTools": "AGP default; not pinned in repository",
      "composeBom": "2026.02.01",
      "androidStudio": "Quail 4 (user-reported)",
      "installedJdk": "OpenJDK 25.0.3+ (user-reported; distinct from Gradle runtime)",
      "testDevice": "Samsung SM-A176B / Android 15 (user-reported; no device test claimed)",
      "branch": "main",
      "workflow": "Feature branches and reviewed pull requests; main is the only observed current branch",
      "unitTestCommand": ".\\gradlew.bat :app:testDebugUnitTest",
      "buildCommand": ".\\gradlew.bat :app:assembleDebug",
      "lintCommand": ".\\gradlew.bat :app:lintDebug",
      "junit": "4.13.2",
      "androidxTestJunit": "1.3.0",
      "espresso": "3.7.0",
      "lifecycle": "2.11.0",
      "activityCompose": "1.13.0"
    },
    "features": {
      "kotlin": {
        "label": "Kotlin",
        "category": "languages",
        "selection": "kotlin",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "compose": {
        "label": "Jetpack Compose",
        "category": "frameworks",
        "selection": "jetpack-compose",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "viewmodel": {
        "label": "AndroidX ViewModel",
        "category": "frameworks",
        "selection": "androidx-viewmodel",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "stateflow": {
        "label": "Coroutines / StateFlow",
        "category": "frameworks",
        "selection": "kotlin-coroutines",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "ide": {
        "label": "Android Studio",
        "category": "ides",
        "selection": "android-studio",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "sdk": {
        "label": "Android SDK",
        "category": "developmentTools",
        "selection": "android-sdk",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "adb": {
        "label": "adb / platform tools",
        "category": "developmentTools",
        "selection": "android-platform-tools",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "build-tools": {
        "label": "Android SDK build tools",
        "category": "developmentTools",
        "selection": "android-build-tools",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "wrapper": {
        "label": "Gradle Wrapper",
        "category": "developmentTools",
        "selection": "gradle-wrapper",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "jdk": {
        "label": "Gradle runtime JDK",
        "category": "toolchains",
        "selection": "java-jdk",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "gradle": {
        "label": "Gradle build system",
        "category": "buildSystems",
        "selection": "gradle",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "dependencies": {
        "label": "Gradle dependencies",
        "category": "dependencyManagers",
        "selection": "gradle",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "git": {
        "label": "Git",
        "category": "versionControlSystems",
        "selection": "git",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "github": {
        "label": "GitHub",
        "category": "developmentTools",
        "selection": "github",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "ci": {
        "label": "GitHub Actions",
        "category": "automationCapabilities",
        "selection": "github-actions",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "junit": {
        "label": "JUnit 4 unit tests",
        "category": "testingCapabilities",
        "selection": "junit",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "instrumentation": {
        "label": "Android instrumentation / Compose UI tests",
        "category": "testingCapabilities",
        "selection": "android-instrumentation",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "lint": {
        "label": "Android lint",
        "category": "qualityCapabilities",
        "selection": "linting",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "android": {
        "label": "Android target",
        "category": "targetPlatforms",
        "selection": "android",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "app-container": {
        "label": "Manual AppContainer dependency injection",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "repository-pattern": {
        "label": "Repository pattern",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "in-memory-data": {
        "label": "Temporary in-memory product and location data",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "walkable-graph": {
        "label": "Walkable graph / map",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "a-star": {
        "label": "A* pathfinding",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "shopping-route": {
        "label": "Shopping-route planner",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "route-recalculation": {
        "label": "Route progress and off-route recalculation",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      },
      "emulator": {
        "label": "Android Emulator / AVD",
        "category": "developmentTools",
        "selection": "android-emulator",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "available environment; no execution evidence",
        "note": ""
      },
      "device": {
        "label": "Connected-device testing",
        "category": "testingCapabilities",
        "selection": "android-device-testing",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "available environment; no execution evidence",
        "note": ""
      },
      "backend": {
        "label": "Backend / API networking",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "planned",
        "provenance": "handover future plans",
        "note": ""
      },
      "positioning": {
        "label": "Integrated live positioning",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "planned",
        "provenance": "handover future plans",
        "note": ""
      },
      "wifi-rtt": {
        "label": "Wi-Fi RTT on the reported test phone",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "user-reported device limitation",
        "note": ""
      },
      "retrofit": {
        "label": "Retrofit",
        "category": "frameworks",
        "selection": "retrofit",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "fastapi": {
        "label": "FastAPI",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "sqlite": {
        "label": "SQLite",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "github-api": {
        "label": "GitHub API application integration",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "navigation-library": {
        "label": "Navigation library",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "di-library": {
        "label": "Dependency injection library",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "unsupported",
        "provenance": "not an existing StorePath dependency",
        "note": ""
      },
      "sdk-framework": {
        "label": "Android SDK framework",
        "category": "frameworks",
        "selection": "android-sdk",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "repository",
        "note": ""
      }
    },
    "rules": [
      "coding-standards",
      "unit-testing",
      "build-must-pass",
      "secret-handling",
      "no-invented-verification"
    ],
    "validation": [
      ".\\gradlew.bat :app:testDebugUnitTest",
      ".\\gradlew.bat :app:assembleDebug",
      ".\\gradlew.bat :app:lintDebug",
      "Instrumentation, emulator and connected-device tests require separate observed evidence."
    ],
    "instructions": [
      "Source observations describe the pinned StorePath revision, not proof that a newly generated project implements these capabilities.",
      "StorePath does not use ARAMF. This selectable ARAMF reference profile does not install ARAMF into StorePath.",
      "Use manual AppContainer injection; do not add navigation or DI libraries as defaults.",
      "Use kotlin.code.style=official, Gradle configuration cache, and repository pattern with ViewModel / StateFlow. Kotlin JVM target 11 is inferred from AGP built-in Kotlin defaults, not an explicit repository pin; see https://developer.android.com/build/migrate-to-built-in-kotlin.",
      "CI runs on push and pull_request to main with contents: read; tasks testDebugUnitTest, assembleDebug and lintDebug.",
      "Source: app/build.gradle.kts, gradle/libs.versions.toml, gradle/wrapper/gradle-wrapper.properties, gradle/gradle-daemon-jvm.properties and .github/workflows/android-ci.yml."
    ]
  },
  "android-application-lab-2": {
    "name": "Android Application Lab 2",
    "source": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
    "settings": {
      "alertDetectionDeadlineSeconds": "30",
      "branchProtectionRequirement": "Course prerequisite: direct PUT to main must be permitted. Review repository policy separately; this profile does not change settings.",
      "normalState": "Normal (green)",
      "alertState": "SecurityAlert (red)",
      "forceMerge": "GET house_config.json; set temperature=17.0 and last_updated_by=Android-Operator; PUT to main; close PR",
      "forceReject": "PATCH pull request state=closed",
      "apiMethods": "POST, PATCH, PUT using Retrofit; GET readback",
      "credentialPolicy": "Classic PAT repo scope is a course requirement; provision outside ARAMF. Never save a token in configuration or generated files."
    },
    "features": {
      "kotlin": {
        "label": "Kotlin",
        "category": "languages",
        "selection": "kotlin",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "compose": {
        "label": "Jetpack Compose",
        "category": "frameworks",
        "selection": "jetpack-compose",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "viewmodel": {
        "label": "AndroidX ViewModel",
        "category": "frameworks",
        "selection": "androidx-viewmodel",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "stateflow": {
        "label": "Coroutines / StateFlow",
        "category": "frameworks",
        "selection": "kotlin-coroutines",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "Available reactive-state option; not explicitly required by Lab 2",
        "note": ""
      },
      "ide": {
        "label": "Android Studio",
        "category": "ides",
        "selection": "android-studio",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "sdk": {
        "label": "Android SDK",
        "category": "developmentTools",
        "selection": "android-sdk",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "adb": {
        "label": "adb / platform tools",
        "category": "developmentTools",
        "selection": "android-platform-tools",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "wrapper": {
        "label": "Gradle Wrapper",
        "category": "developmentTools",
        "selection": "gradle-wrapper",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "jdk": {
        "label": "Gradle runtime JDK",
        "category": "toolchains",
        "selection": "java-jdk",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "gradle": {
        "label": "Gradle build system",
        "category": "buildSystems",
        "selection": "gradle",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "dependencies": {
        "label": "Gradle dependencies",
        "category": "dependencyManagers",
        "selection": "gradle",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "git": {
        "label": "Git",
        "category": "versionControlSystems",
        "selection": "git",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      },
      "github": {
        "label": "GitHub",
        "category": "developmentTools",
        "selection": "github",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "android": {
        "label": "Android target",
        "category": "targetPlatforms",
        "selection": "android",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "retrofit": {
        "label": "Retrofit",
        "category": "frameworks",
        "selection": "retrofit",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "human-approval": {
        "label": "Explicit human Force Merge / Force Reject approval",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "api-writes": {
        "label": "GitHub REST writes",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "defensive-errors": {
        "label": "Wrap API writes in try-catch; handle network drops and HTTP 401 / 403 / 422 using readable Snackbar; never crash",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "incident-closure": {
        "label": "Close PR and poll ViewModel back to Normal green",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "private-repository": {
        "label": "Private coursework repository; configuration performed separately",
        "category": "",
        "selection": "",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "Android-Application-LAB-2.txt (provided coursework; SHA-256 738bcc4e2abb33df1485166601aeb2df77e4c6a6fc79d310f46825ee48400d84)",
        "note": ""
      },
      "sdk-framework": {
        "label": "Android SDK framework",
        "category": "frameworks",
        "selection": "android-sdk",
        "required": false,
        "selected": true,
        "availability": "available",
        "provenance": "Android development default; confirm against coursework application repository",
        "note": ""
      }
    },
    "rules": [
      "coding-standards",
      "unit-testing",
      "build-must-pass",
      "secret-handling",
      "no-invented-verification",
      "error-handling",
      "authentication-authorization",
      "ask-high-risk"
    ],
    "validation": [
      "Verify detection within 30 seconds",
      "Verify Force Reject closes the PR after human approval",
      "Verify Force Merge changes house_config.json then closes the PR after human approval",
      "Verify ViewModel polling returns to Normal green",
      "Verify network failures and HTTP 401/403/422 show a readable Snackbar without a crash"
    ],
    "instructions": [
      "These are coursework requirements, not claims of implemented features. Display the deceptive attack text with the red SecurityAlert before the operator decides.",
      "Do not perform API writes, change repository settings, provision credentials or add collaborators while configuring this profile.",
      "No SDK, AGP, Kotlin or Gradle version is invented from this lab. Resolve versions from the selected application repository."
    ]
  },
  "keyestudio-smart-home-kit": {
    "name": "Keyestudio Smart Home Kit",
    "source": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
    "settings": {
      "board": "Keyestudio PLUS / Arduino UNO compatible",
      "shield": "Sensor shield V5.2",
      "bluetooth": "HM-10 BLE to UART",
      "arduinoIdeVersion": "Choose installed supported version; 1.8.12 is a historical guide example"
    },
    "features": {
      "cpp": {
        "label": "C/C++ firmware",
        "category": "languages",
        "selection": "cpp",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "ide": {
        "label": "Arduino IDE",
        "category": "ides",
        "selection": "arduino-ide",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "framework": {
        "label": "Arduino",
        "category": "frameworks",
        "selection": "arduino",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "compiler": {
        "label": "AVR-GCC",
        "category": "toolchains",
        "selection": "avr-gcc",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "build": {
        "label": "Arduino build",
        "category": "buildSystems",
        "selection": "arduino-build",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "target": {
        "label": "Microcontroller",
        "category": "targetPlatforms",
        "selection": "microcontroller",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "architecture": {
        "label": "AVR",
        "category": "targetArchitectures",
        "selection": "avr",
        "required": true,
        "selected": true,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "hardware": {
        "label": "LED, button, photocell, PIR, MQ2 gas, relay, buzzer, fan, steam, servo, LCD1602, soil humidity",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "libraries": {
        "label": "Servo and LiquidCrystal_I2C supplied libraries",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "driver": {
        "label": "CP2102 USB driver / installation guides",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      },
      "examples": {
        "label": "15 supplied Arduino projects",
        "category": "",
        "selection": "",
        "required": false,
        "selected": false,
        "availability": "available",
        "provenance": "KS0085 Smart Home Kit for Arduino.zip; KS0085 Keyestudio Smart Home Kit for Arduino.pdf; provided product PDF",
        "note": ""
      }
    },
    "rules": [
      "coding-standards",
      "unit-testing",
      "build-must-pass",
      "secret-handling",
      "no-invented-verification"
    ],
    "validation": [
      "Compile the selected Arduino sketch using its supported board configuration",
      "Test sensors, actuators and safe states separately on physical hardware",
      "Verify HM-10 pairing and UART communication on hardware before claiming integration"
    ],
    "instructions": [
      "Optional hardware profile; it does not add Android or hardware requirements to StorePath.",
      "Supplied tutorials and examples are resources, not physical verification evidence. Do not invent pin assignments."
    ]
  }
})ARAMF")).object();
    return profiles.value(id).toObject();
}

QList<EnvironmentOption> EnvironmentCatalog::projectProfiles()
// Brief: List independent profiles; none releases defaults without deleting user edits.
{
    return {{"No named profile", ""}, {"StorePath Android", "storepath-android"}, {"Android Application Lab 2", "android-application-lab-2"}, {"Keyestudio Smart Home Kit (optional hardware)", "keyestudio-smart-home-kit"}};
}
