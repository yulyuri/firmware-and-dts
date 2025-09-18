# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "debug")
  file(REMOVE_RECURSE
  "output.map"
  "rpmsg_lite_str_echo_rtos_remote.bin"
  )
endif()
