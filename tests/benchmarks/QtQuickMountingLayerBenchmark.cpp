// Mutation-only host benchmark; excludes creation and rendering.
// Run zero warms each process; compare runs one through five.
#include "react-native-qnx/fabric/QtQuickMountingLayer.h"
#include "react-native-qnx/views/QtQuickItemFactory.h"
#include <QGuiApplication>
#include <QQmlEngine>
#include <QQuickItem>
#include <chrono>
#include <cstdio>
#include <cstdlib>
using namespace rnqnx::fabric;
using Clock = std::chrono::steady_clock;
void check(bool ok) { if (!ok) std::abort(); }
void ok(MountError e) { check(e == MountError::None); }
double ms(Clock::time_point start) { return std::chrono::duration<double,std::milli>(Clock::now()-start).count(); }
struct Fixture {
 QQmlEngine engine;
 QQuickItem root;
 rnqnx::views::QtQuickItemFactory factory{engine};
 QtQuickMountingLayer layer{factory,root,1};
 Fixture() {factory.registerType(QStringLiteral("View"),QByteArray("import QtQuick\nItem {}\n"));}
 void create(Tag t) {ok(layer.create(t,QStringLiteral("View")));}
};
int main(int argc,char**argv) {
 QGuiApplication app(argc,argv);
 constexpr int n=1000;
 for(int run=0;run<6;++run) {
  {
   Fixture f;
   for(int i=0;i<n;++i) f.create(i+2);
   auto t=Clock::now();
   for(int i=0;i<n;++i) ok(f.layer.insert(1,i+2,i));
   double insert=ms(t);
   const auto children=f.root.childItems();check(children.size()==n);
   for(int i=0;i<n;++i) check(children[i]==f.layer.itemForTag(i+2));
   t=Clock::now();
   for(int i=0;i<n;++i) ok(f.layer.remove(1,i+2,0));
   double remove=ms(t);check(f.root.childItems().isEmpty());
   std::printf("{\"workload\":\"siblings\",\"run\":%d,\"nodes\":%d,\"insertMs\":%.6f,\"removeMs\":%.6f}\n",run,n,insert,remove);
  }
  {
   Fixture f;f.create(2);f.create(3);ok(f.layer.insert(1,2,0));ok(f.layer.insert(1,3,1));
   for(int i=0;i<n;++i) {f.create(i+4);ok(f.layer.insert(3,i+4,i));}
   f.create(n+4);auto t=Clock::now();
   for(int i=0;i<n;++i) {ok(f.layer.insert(2,n+4,0));ok(f.layer.remove(2,n+4,0));}
   double toggle=ms(t);
   check(f.layer.itemForTag(2)->childItems().isEmpty());check(f.layer.itemForTag(3)->childItems().size()==n);
   std::printf("{\"workload\":\"unrelated\",\"run\":%d,\"nodes\":%d,\"toggleMs\":%.6f}\n",run,n,toggle);
  }
 }
}
