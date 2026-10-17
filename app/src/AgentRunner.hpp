#pragma once
namespace astro::app {
// Called only by the explicit --agent entry point; never creates a QML engine.
int runAgent(int argc, char** argv);
}
