cmake_minimum_required(VERSION 3.16)
project(codeck_host_tests C)
enable_testing()
get_filename_component(ROOT "../.." ABSOLUTE BASE_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(CJSON "${ROOT}/managed_components/espressif__cjson/cJSON")
if(NOT EXISTS "${CJSON}/cJSON.c")
    message(FATAL_ERROR "Run idf.py build first to download managed components")
endif()
add_library(quota_data "${ROOT}/main/network/quota_data.c" "${CJSON}/cJSON.c")
target_include_directories(quota_data PUBLIC "${ROOT}/main/network/include" "${CJSON}")
target_compile_definitions(quota_data PUBLIC _POSIX_C_SOURCE=200809L CJSON_NESTING_LIMIT=32)
target_compile_options(quota_data PRIVATE -Wall -Wextra -Werror)
target_link_libraries(quota_data PUBLIC m)
add_executable(test_quota test_quota.c)
target_link_libraries(test_quota PRIVATE quota_data)
add_test(NAME quota COMMAND test_quota)
add_executable(test_account_client test_account_client.c)
target_include_directories(test_account_client PRIVATE client_stubs stubs)
target_compile_options(test_account_client PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_account_client PRIVATE quota_data)
add_test(NAME account_client COMMAND test_account_client)

set(LV_BUILD_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE PATH "" FORCE)
set(CONFIG_LV_BUILD_DEMOS OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CONFIG_LV_USE_THORVG_INTERNAL OFF CACHE BOOL "" FORCE)
add_subdirectory("${ROOT}/managed_components/lvgl__lvgl" lvgl)
add_executable(test_ui test_ui.c "${ROOT}/main/ui/ui_home.c"
    "${ROOT}/main/ui/ui_dashboard.c"
    "${ROOT}/main/input/lvgl_touch_input.c"
    "${ROOT}/main/ui/font/ui_home_zh_20.c"
    "${ROOT}/main/ui/font/ui_dashboard_zh_16.c"
    "${ROOT}/main/ui/font/ui_cost_digits_64.c")
target_include_directories(test_ui PRIVATE stubs "${ROOT}/main/ui/include"
    "${ROOT}/main/input/include"
    "${ROOT}/managed_components/lvgl__lvgl/src")
target_link_libraries(test_ui PRIVATE quota_data lvgl)
add_test(NAME ui COMMAND test_ui)
add_test(NAME ui_device_heap COMMAND test_ui)
set_tests_properties(ui_device_heap PROPERTIES ENVIRONMENT "CODECK_UI_HEAP_BYTES=131072" TIMEOUT 15)

foreach(name device_config network_manager)
    add_executable(test_${name} test_${name}.c)
    target_include_directories(test_${name} PRIVATE stubs "${ROOT}/main/network/include")
    target_compile_options(test_${name} PRIVATE -Wall -Wextra -Werror)
    add_test(NAME ${name} COMMAND test_${name})
endforeach()

add_executable(test_provision_server test_provision_server.c "${ROOT}/main/network/device_config.c")
target_include_directories(test_provision_server PRIVATE stubs "${ROOT}/main/network/include")
target_compile_definitions(test_provision_server PRIVATE _DEFAULT_SOURCE)
target_compile_options(test_provision_server PRIVATE -Wall -Wextra -Werror)
target_link_libraries(test_provision_server PRIVATE quota_data)
add_test(NAME provision_server COMMAND test_provision_server)

find_program(NODE_EXECUTABLE node)
if(NODE_EXECUTABLE)
    add_test(NAME provision_web COMMAND "${NODE_EXECUTABLE}" "${CMAKE_CURRENT_SOURCE_DIR}/test_provision_web.js")
endif()
