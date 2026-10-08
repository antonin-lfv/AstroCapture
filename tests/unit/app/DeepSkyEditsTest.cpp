#include <QCoreApplication>
#include "DeepSkyEdits.hpp"
#include "Check.hpp"
#include <limits>
using namespace astro::app;
int main(int argc,char** argv){
    QCoreApplication application(argc,argv);DeepSkyEdits edits;
    const auto initial=edits.values();auto next=initial;
    next["midtones"]=.3;ASTRO_TEST_CHECK(edits.applyValues(next)&&edits.canUndo());
    edits.undo();ASTRO_TEST_CHECK(edits.values()==initial&&edits.canRedo());
    edits.redo();ASTRO_TEST_CHECK(edits.values()==next);
    edits.beginEdit();next["midtones"]=.4;edits.applyValues(next);next["midtones"]=.5;edits.applyValues(next);edits.endEdit();
    edits.undo();ASTRO_TEST_CHECK(edits.values()["midtones"]==.3);
    edits.redo();edits.selectVersion("B");ASTRO_TEST_CHECK(edits.values()==initial&&!edits.canUndo());
    next=initial;next["saturation"]=1.7;edits.applyValues(next);
    edits.selectVersion("A");ASTRO_TEST_CHECK(edits.values()["midtones"]==.5);
    edits.copyToOther();edits.selectVersion("B");ASTRO_TEST_CHECK(edits.values()["midtones"]==.5);
    edits.undo();ASTRO_TEST_CHECK(edits.values()==next);
    const auto snapshot=edits.snapshot();DeepSkyEdits restored;
    ASTRO_TEST_CHECK(restored.restoreSession(snapshot)&&restored.version()=="B"&&restored.values()==next&&!restored.canUndo());
    restored.selectVersion("A");ASTRO_TEST_CHECK(restored.values()["midtones"]==.5);
    auto endpoint=initial;endpoint["midtones"]=.999f;ASTRO_TEST_CHECK(checkedDeepSkySettings(endpoint));
    auto invalid=initial;invalid["white"]=0;ASTRO_TEST_CHECK(!edits.applyValues(invalid));
    invalid=initial;invalid["red"]=std::numeric_limits<double>::quiet_NaN();ASTRO_TEST_CHECK(!edits.applyValues(invalid));
    invalid=initial;invalid["asinh"]=1;ASTRO_TEST_CHECK(!edits.applyValues(invalid));
    invalid=initial;invalid.remove("black");ASTRO_TEST_CHECK(!edits.applyValues(invalid));
    auto corrupt=snapshot;corrupt["B"]=invalid;ASTRO_TEST_CHECK(!restored.restoreSession(corrupt));
    edits.resetSession(initial);ASTRO_TEST_CHECK(!edits.canUndo()&&!edits.canRedo()&&edits.version()=="A");
    for(int n=0;n<120;++n){next=initial;next["saturation"]=n/100.;edits.applyValues(next);}
    int count=0;while(edits.canUndo()){edits.undo();++count;}ASTRO_TEST_CHECK(count==100);
    edits.redo();next=initial;next["saturation"]=2.8;edits.applyValues(next);ASTRO_TEST_CHECK(!edits.canRedo());
    std::cout<<"OK\n";
}
