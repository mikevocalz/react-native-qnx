#include <worklets/Tools/PlatformLogger.h>
#include <QDebug>
namespace worklets {
void PlatformLogger::log(const char* value) { qInfo().noquote() << value; }
void PlatformLogger::log(const std::string& value) { log(value.c_str()); }
void PlatformLogger::log(double value) { qInfo() << value; }
void PlatformLogger::log(int value) { qInfo() << value; }
void PlatformLogger::log(bool value) { qInfo() << value; }
}
