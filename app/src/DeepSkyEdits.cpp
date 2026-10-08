#include "DeepSkyEdits.hpp"

namespace astro::app {
void DeepSkyEdits::push(State& state, const QVariantMap& values) {
    if (state.undo.size() == 100) state.undo.erase(state.undo.begin());
    state.undo.push_back(values);
    state.redo.clear();
}
void DeepSkyEdits::beginEdit() {
    auto& state = states_[active_];
    if (!state.gesture) state.gesture = state.values;
}
void DeepSkyEdits::endEdit() {
    auto& state = states_[active_];
    if (state.gesture && *state.gesture != state.values) push(state, *state.gesture);
    state.gesture.reset();
    emit historyChanged();
}
bool DeepSkyEdits::applyValues(const QVariantMap& input) {
    const auto checked = checkedDeepSkySettings(input);
    if (!checked) return false;
    auto& state = states_[active_];
    if (*checked == state.values) return true;
    if (!state.gesture) push(state, state.values);
    state.values = *checked;
    notify();
    return true;
}
void DeepSkyEdits::undo() {
    endEdit();
    auto& state = states_[active_];
    if (state.undo.empty()) return;
    state.redo.push_back(state.values);
    state.values = std::move(state.undo.back());
    state.undo.pop_back();
    notify();
}
void DeepSkyEdits::redo() {
    endEdit();
    auto& state = states_[active_];
    if (state.redo.empty()) return;
    state.undo.push_back(state.values);
    state.values = std::move(state.redo.back());
    state.redo.pop_back();
    notify();
}
bool DeepSkyEdits::resetSession(const QVariantMap& input) {
    const auto checked = checkedDeepSkySettings(input);
    if (!checked) return false;
    states_ = {State{}, State{}};
    states_[0].values = states_[1].values = *checked;
    active_ = 0;
    notify();
    return true;
}
void DeepSkyEdits::selectVersion(const QString& version) {
    if (version != "A" && version != "B") return;
    const int next = version == "A" ? 0 : 1;
    if (next == active_) return;
    endEdit();
    active_ = next;
    notify();
}
void DeepSkyEdits::copyToOther() {
    endEdit();
    auto& other = states_[1 - active_];
    if (other.values == values()) return;
    push(other, other.values);
    other.values = values();
    other.gesture.reset();
    emit historyChanged();
}
QVariantMap DeepSkyEdits::snapshot() const {
    return {{"A", states_[0].values}, {"B", states_[1].values}, {"active", version()}};
}
bool DeepSkyEdits::restoreSession(const QVariantMap& input) {
    const auto checked = checkedDeepSkySnapshot(input);
    if (!checked) return false;
    states_ = {State{}, State{}};
    states_[0].values = checked->value("A").toMap();
    states_[1].values = checked->value("B").toMap();
    active_ = checked->value("active") == "A" ? 0 : 1;
    notify();
    return true;
}
}
