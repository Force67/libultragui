# SVG loader test (no GPU required)
add_executable(test_svg test_svg.cc)
target_link_libraries(test_svg ultragui)
add_test(NAME svg COMMAND test_svg)

# Audio engine test
if(ULTRAGUI_AUDIO)
    add_executable(test_audio test_audio.cc)
    target_link_libraries(test_audio ultragui)
    add_test(NAME audio COMMAND test_audio)
endif()

# Lottie animation test
if(ULTRAGUI_LOTTIE)
    add_executable(test_lottie test_lottie.cc)
    target_link_libraries(test_lottie ultragui rlottie)
    add_test(NAME lottie COMMAND test_lottie)
endif()
