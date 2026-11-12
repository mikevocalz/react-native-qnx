# cmake -DDESCRIPTOR=<json> -DOUT=<dir> [-DEXPECT_ERROR=<text>] -P ConsumerDescriptorCheck.cmake
# Runs the consumer `window` reader on one descriptor. A positive case writes
# the generated header; a negative case must fail with EXPECT_ERROR.
include("${CMAKE_CURRENT_LIST_DIR}/../../cmake/RnqnxConsumerApp.cmake")
file(READ "${DESCRIPTOR}" config)
file(MAKE_DIRECTORY "${OUT}")
rnqnx_consumer_window_chrome("${config}" "${OUT}" header)
message(STATUS "RNQNX_WINDOW_HEADER ${header}")
