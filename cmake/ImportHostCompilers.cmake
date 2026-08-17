# Imported host Hermes compiler executables used for cross-compilation.
# This avoids a full host Hermes build when a prebuilt hermesc is available.

set(_HERMESC_DIR "C:/Users/Owner/react-native-qnx/node_modules/hermes-compiler/hermesc/win64-bin")

add_executable(imported-hermesc IMPORTED)
set_target_properties(imported-hermesc PROPERTIES
  IMPORTED_LOCATION "${_HERMESC_DIR}/hermesc.exe"
)

# shermes is not shipped with hermes-compiler, but Hermes' CMake references it
# when HERMESVM_INTERNAL_JAVASCRIPT_NATIVE is ON. We set it to the same hermesc
# executable as a placeholder; with HERMESVM_INTERNAL_JAVASCRIPT_NATIVE=OFF the
# natively-compiled internal unit is not built, so this target is not invoked.
add_executable(imported-shermes IMPORTED)
set_target_properties(imported-shermes PROPERTIES
  IMPORTED_LOCATION "${_HERMESC_DIR}/hermesc.exe"
)
