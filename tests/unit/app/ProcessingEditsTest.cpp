#include <QCoreApplication>
#include <limits>
#include "ProcessingEdits.hpp"
#include "Check.hpp"
int main(int argc, char** argv) {
    auto smallCrop=astro::app::defaultGeometry();smallCrop["crop"]=true;smallCrop["width"]=512;smallCrop["height"]=512;smallCrop["scale"]=2;
    ASTRO_TEST_CHECK(astro::app::geometryWorkingBytes(12488,8340,smallCrop)<64ULL*1024*1024);

    QCoreApplication app(argc, argv);
    astro::app::ProcessingEdits edits;
    const auto neutral = edits.values();
    ASTRO_TEST_CHECK(!edits.canUndo() && !edits.canRedo());
    edits.beginEdit();
    edits.setValue("level1", 1.0);
    edits.setValue("level1", 2.0);
    edits.setValue("level1", 3.0);
    edits.endEdit();
    ASTRO_TEST_CHECK(edits.values()["level1"].toDouble() == 3.0 && edits.canUndo());
    edits.undo(); // One drag is one step, not hundreds of slider events.
    ASTRO_TEST_CHECK(edits.values() == neutral && !edits.canUndo() && edits.canRedo());
    edits.redo();
    ASTRO_TEST_CHECK(edits.values()["level1"].toDouble() == 3.0);
    auto preset = neutral;
    preset["gamma"] = 1.4; preset["red"] = 1.2;
    ASTRO_TEST_CHECK(edits.applyValues(preset));
    edits.undo();
    ASTRO_TEST_CHECK(edits.values()["level1"].toDouble() == 3.0 && edits.values()["gamma"].toDouble() == 1.0);
    edits.setValue("blue", 0.9);
    ASTRO_TEST_CHECK(!edits.canRedo()); // A new branch discards redo.
    edits.setValue("level1", 10);
    ASTRO_TEST_CHECK(edits.values()["level1"].toDouble() == 10);
    edits.setValue("level1", 50);
    ASTRO_TEST_CHECK(edits.values()["level1"].toDouble() == 50);
    const auto valid = edits.values();
    edits.setValue("level1", 50.01);
    edits.setValue("level4", 10.01);
    edits.setValue("red", std::numeric_limits<double>::quiet_NaN());
    edits.setValue("level1", 100);
    edits.setValue("missing", 1);
    ASTRO_TEST_CHECK(!edits.applyValues({{"gamma", 1.4}}));
    ASTRO_TEST_CHECK(edits.values() == valid);
    edits.reset(); edits.undo();
    ASTRO_TEST_CHECK(edits.values() == valid); // Reset is reversible.
    edits.beginEdit(); edits.setValue("level2", 2); edits.undo();
    ASTRO_TEST_CHECK(edits.values() == valid); // Undo finishes an in-progress edit safely.
    edits.resetSession();
    ASTRO_TEST_CHECK(edits.values() == neutral && !edits.canUndo() && !edits.canRedo());
    for (int i = 0; i < 120; ++i) edits.setValue("level1", i % 2 ? 1.0 : 2.0);
    int count = 0;
    while (edits.canUndo()) { edits.undo(); ++count; }
    ASTRO_TEST_CHECK(count == 100); // Bounded parameter history.
}
