#pragma once
#include "QtWorkletsScheduler.h"
#include <QMetaObject>
#include <QPointer>
#include <QQuickWindow>
#include <functional>
#include <memory>
#include <vector>
namespace rnqnx::runtime {
// One borrowed window, attached before bundle initialization. A frameSwapped
// observation drives callbacks; this is not a physical presentation timestamp.
class QtWorkletsFrameSource final : public std::enable_shared_from_this<QtWorkletsFrameSource> {
 public:
  explicit QtWorkletsFrameSource(std::shared_ptr<QtWorkletsScheduler> scheduler);
  ~QtWorkletsFrameSource();
  void attach(QQuickWindow* window);
  void request(std::function<void(double)> callback);
  void close();
  bool ready() const;
 private:
  void deliver(double timestamp);
  void requireOwner() const;
  std::shared_ptr<QtWorkletsScheduler> scheduler_;
  QPointer<QQuickWindow> window_;
  QMetaObject::Connection swapped_;
  QMetaObject::Connection destroyed_;
  std::vector<std::function<void(double)>> pending_;
  bool attached_{false};
  bool closed_{false};
};
}
