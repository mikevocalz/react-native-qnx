#include <hermes/hermes.h>
#include <jsi/jsi.h>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
int main() {
 auto runtime=facebook::hermes::makeHermesRuntime();
 auto evaluate=[&](const std::string& source) {
  return runtime->evaluateJavaScript(std::make_unique<facebook::jsi::StringBuffer>(source), "window-api-host-test.js");
 };
 evaluate("const lexicalCheck = (ok) => { if (!ok) throw new Error('assertion'); };");
 bool lexicalFailed=false;
 try { evaluate("lexicalCheck(true);"); } catch (const facebook::jsi::JSError& e) {
  lexicalFailed=std::string(e.what()).find("lexicalCheck")!=std::string::npos;
  std::cout << "EXPECTED_ORIGINAL_HERMES_SCOPE_FAILURE " << e.what() << '\n';
 }
 if (!lexicalFailed) throw std::runtime_error("negative control did not reproduce lexical scope loss");
 evaluate("globalThis.check = (ok, message) => { if (!ok) throw new Error(message); };");
 evaluate("check(true, 'must pass in another evaluation');");
 bool assertionFailed=false;
 try { evaluate("check(false, 'intentional negative assertion');"); } catch (const facebook::jsi::JSError& e) {
  assertionFailed=std::string(e.what()).find("intentional negative assertion")!=std::string::npos;
 }
 if (!assertionFailed) throw std::runtime_error("assertion was weakened");
 std::cout << "WINDOW_API_HELPER_SCOPE_OK separate Hermes programs preserve global helper and reject false assertions\n";
}
