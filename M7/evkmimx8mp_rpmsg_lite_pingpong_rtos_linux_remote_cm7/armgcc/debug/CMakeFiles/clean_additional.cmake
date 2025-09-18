# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "debug")
  file(REMOVE_RECURSE
  "output.map"
  "rpmsg_lite_pingpong_rtos_linux_remote.bin"
  )
endif()
