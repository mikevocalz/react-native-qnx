execute_process(COMMAND "${PROBE}" --json RESULT_VARIABLE status
  OUTPUT_VARIABLE output ERROR_VARIABLE error TIMEOUT 12)
if(NOT "${status}" STREQUAL "0")
  message(FATAL_ERROR "Trace export failed: ${status}\n${error}")
endif()
string(JSON schema GET "${output}" schemaVersion)
string(JSON dropped GET "${output}" dropped)
string(JSON count LENGTH "${output}" events)
string(JSON stage GET "${output}" events 0 stage)
string(JSON correlation GET "${output}" events 0 correlation)
if(NOT schema EQUAL 1 OR NOT dropped EQUAL 0 OR NOT count EQUAL 1 OR
   NOT stage STREQUAL "queue.admit" OR NOT correlation EQUAL 17)
  message(FATAL_ERROR "Unexpected trace JSON schema/event data")
endif()
