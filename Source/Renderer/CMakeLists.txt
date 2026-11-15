cmake_minimum_required(VERSION 3.25)
project(RebornRenderer LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
add_library(RebornRenderer SHARED renderer_d3d11.cpp)
target_compile_definitions(RebornRenderer PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(RebornRenderer PRIVATE d3d11 dxgi d3dcompiler gdiplus)
target_compile_options(RebornRenderer PRIVATE /W4 /EHsc)
add_executable(renderer_test renderer_test.cpp)
target_compile_definitions(renderer_test PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
target_link_libraries(renderer_test PRIVATE RebornRenderer gdiplus)
enable_testing()
add_test(NAME native_renderer COMMAND renderer_test)
add_executable(renderer_unload_test renderer_unload_test.cpp)
target_compile_definitions(renderer_unload_test PRIVATE WIN32_LEAN_AND_MEAN NOMINMAX)
add_dependencies(renderer_unload_test RebornRenderer)
add_test(NAME renderer_font_unload COMMAND renderer_unload_test)
set_tests_properties(renderer_font_unload PROPERTIES TIMEOUT 20)
add_custom_command(TARGET renderer_unload_test POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:renderer_unload_test>/Fonts"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/../../Assets/Fonts/ChunkyZorvy-Regular.otf"
        "$<TARGET_FILE_DIR:renderer_unload_test>/Fonts/ChunkyZorvy-Regular.otf")
