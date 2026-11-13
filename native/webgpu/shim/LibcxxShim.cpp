// Defines libc++ members that clang-built objects reference but the SDP's
// libc++.so does not export.
//
// The QNX SDP's libc++.so was built with gcc. Its headers declare these class
// templates `extern`, so clang (which treats the members marked
// exclude_from_explicit_instantiation as inline) still emits calls to
// symbols such as basic_ostringstream<char>::str() const& that the shared
// library never defined. gcc ignores that attribute, so explicitly
// instantiating the classes here, with gcc, emits the missing definitions.
//
// Only needed when Dawn is compiled with cmake/toolchains/qnx-8.0-clang.cmake;
// it is harmless with the gcc toolchain.
#include <fstream>
#include <sstream>

template class std::basic_ostringstream<char>;
template class std::basic_stringstream<char>;
template class std::basic_ifstream<char>;

// clang does not put the ABI tag on constructors, gcc does. Dawn's
// time_zone_info (from Abseil) calls basic_ifstream(const string&, openmode)
// through the untagged name, so alias it to the gcc-tagged definition above.
__asm__(
    ".weak _ZNSt3__214basic_ifstreamIcNS_11char_traitsIcEEEC1ERKNS_12basic_stringIcS2_NS_9allocatorIcEEEEj\n"
    ".set _ZNSt3__214basic_ifstreamIcNS_11char_traitsIcEEEC1ERKNS_12basic_stringIcS2_NS_9allocatorIcEEEEj,"
    " _ZNSt3__214basic_ifstreamIcNS_11char_traitsIcEEEC1B7v160006ERKNS_12basic_stringIcS2_NS_9allocatorIcEEEEj\n");
