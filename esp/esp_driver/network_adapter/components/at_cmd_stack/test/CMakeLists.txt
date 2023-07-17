cmake_minimum_required(VERSION 3.5)

# This example uses an extra component for common functions such as Wi-Fi and 
# Ethernet connection. Also include component directory of at_cmd_stack
set(EXTRA_COMPONENT_DIRS "../../")
list(APPEND EXTRA_COMPONENT_DIRS $ENV{IDF_PATH}/examples/common_components/protocol_examples_common)


set(PROJECT_VERSION "0.4")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(at_cmd_stack_test)

set(IDF_TARGET "esp32s3") 