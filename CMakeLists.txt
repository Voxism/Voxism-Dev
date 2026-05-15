cmake_minimum_required(VERSION 3.12)
include("${CMAKE_SOURCE_DIR}/FindGfxLibs.cmake")

# Name of the project
project(Voxism)

# ---------------------------------------------------------------------------
# Unit / property tests (no OpenGL context required — compile standalone)
# ---------------------------------------------------------------------------
add_executable(test_shader_path src/tests/test_shader_path.cpp)
target_compile_features(test_shader_path PRIVATE cxx_std_14)
enable_testing()
add_test(NAME ShaderPathProperty COMMAND test_shader_path)

add_executable(test_ssao_kernel src/tests/test_ssao_kernel.cpp)
target_compile_features(test_ssao_kernel PRIVATE cxx_std_14)
target_include_directories(test_ssao_kernel PRIVATE ext)
findGLM(test_ssao_kernel)
# Fallback: add system include paths used by the main target (e.g. Homebrew on macOS)
if(APPLE)
    target_include_directories(test_ssao_kernel SYSTEM PRIVATE /opt/homebrew/include)
endif()
add_test(NAME SSAOKernelHemisphereInvariant COMMAND test_ssao_kernel)

add_executable(test_ssao_bias src/tests/test_ssao_bias.cpp)
target_compile_features(test_ssao_bias PRIVATE cxx_std_14)
target_include_directories(test_ssao_bias PRIVATE ext)
findGLM(test_ssao_bias)
if(APPLE)
    target_include_directories(test_ssao_bias SYSTEM PRIVATE /opt/homebrew/include)
endif()
add_test(NAME SSAOKernelDistributionBias COMMAND test_ssao_bias)

add_executable(test_noise_vectors src/tests/test_noise_vectors.cpp)
target_compile_features(test_noise_vectors PRIVATE cxx_std_14)
target_include_directories(test_noise_vectors PRIVATE ext)
findGLM(test_noise_vectors)
if(APPLE)
    target_include_directories(test_noise_vectors SYSTEM PRIVATE /opt/homebrew/include)
endif()
add_test(NAME NoiseVectorNormalization COMMAND test_noise_vectors)

# Enforce use of C++14
set(CMAKE_CXX_STANDARD_REQUIRED 14)
set(CMAKE_CXX_STANDARD 14)

# Use glob to get the list of all source files.
# CONFIGURE_DEPENDS makes CMake re-check globs on each build so newly added
# files (e.g. src/world/*.cpp) are picked up without manually re-running cmake.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    "${CMAKE_SOURCE_DIR}/src/*.cpp"
    "${CMAKE_SOURCE_DIR}/src/audio/*.c"
    "${CMAKE_SOURCE_DIR}/ext/*/*.cpp"
    "${CMAKE_SOURCE_DIR}/ext/glad/src/*.c")
# Exclude standalone test files (they define their own main())
list(FILTER SOURCES EXCLUDE REGEX ".*/src/tests/.*\\.cpp$")

# We don't really need to include header and resource files to build, but it's
# nice to have them show up in IDEs.
file(GLOB_RECURSE HEADERS CONFIGURE_DEPENDS "src/*.h" "ext/*/*.h" "ext/glad/*/*.h")
file(GLOB_RECURSE GLSL CONFIGURE_DEPENDS "resources/*.glsl")

include_directories("ext")
include_directories("ext/glad/include")
include_directories("src/imgui")
include_directories("src/imgui/backends")

# Set the executable.
add_executable(${CMAKE_PROJECT_NAME} ${SOURCES} ${HEADERS} ${GLSL})

# Helper function included from FindGfxLibs.cmake
findGLFW3(${CMAKE_PROJECT_NAME})
findGLM(${CMAKE_PROJECT_NAME})

# OS specific options and libraries
if(NOT WIN32)

  message(STATUS "Adding GCC style compiler flags")
  add_compile_options("-Wall")
  add_compile_options("-pedantic")
  add_compile_options("-Werror=return-type")
 
  # TODO: The following links may be uneeded. 
  if(APPLE)
    # Add required frameworks for GLFW.
    target_link_libraries(${CMAKE_PROJECT_NAME} "-framework OpenGL -framework Cocoa -framework IOKit -framework CoreVideo")
    # miniaudio backends on macOS
    target_link_libraries(${CMAKE_PROJECT_NAME} "-framework CoreAudio -framework AudioToolbox -framework CoreFoundation")
  else()
    #Link the Linux OpenGL library
    target_link_libraries(${CMAKE_PROJECT_NAME} "GL" "dl")
    # miniaudio on Linux needs pthread and libm (dl already linked above)
    find_package(Threads REQUIRED)
    target_link_libraries(${CMAKE_PROJECT_NAME} Threads::Threads "m")
  endif()

else()

  # Link OpenGL on Windows
  target_link_libraries(${CMAKE_PROJECT_NAME} opengl32.lib)
  # miniaudio uses WASAPI on Windows; ole32 provides COM init it relies on.
  target_link_libraries(${CMAKE_PROJECT_NAME} ole32.lib)

endif()
