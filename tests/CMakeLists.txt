# SVG loader test (no GPU required)
add_executable(test_svg test_svg.cc)
target_link_libraries(test_svg ultragui)
add_test(NAME svg COMMAND test_svg)

# Audio engine test. Compiles the wire-in miniaudio backend into the test, the
# same way an application would (the lib no longer bakes it in).
if(ULTRAGUI_AUDIO)
    add_executable(test_audio test_audio.cc
        ${CMAKE_SOURCE_DIR}/ugui/backends/ugui_impl_miniaudio.cc)
    target_include_directories(test_audio
        PRIVATE ${CMAKE_SOURCE_DIR}/third_party/miniaudio)
    target_link_libraries(test_audio ultragui)
    add_test(NAME audio COMMAND test_audio)
endif()

# Lottie animation test
if(ULTRAGUI_LOTTIE)
    add_executable(test_lottie test_lottie.cc)
    target_link_libraries(test_lottie ultragui rlottie)
    add_test(NAME lottie COMMAND test_lottie)
endif()

# Runtime safety regression tests
add_executable(test_runtime_safety test_runtime_safety.cc)
target_link_libraries(test_runtime_safety ultragui)
add_test(NAME runtime_safety COMMAND test_runtime_safety)
