# Additional clean files
cmake_minimum_required(VERSION 3.16)

if("${CONFIG}" STREQUAL "" OR "${CONFIG}" STREQUAL "debug")
  file(REMOVE_RECURSE
  "ii2c_polling_b2b_transfer_master.bin"
  "output.map"
  )
endif()
