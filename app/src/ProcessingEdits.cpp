#include "ProcessingEdits.hpp"

namespace astro::app {
void ProcessingEdits::pushUndo(const QVariantMap& values) {
    if (undo_.size() == 100) undo_.erase(undo_.begin());
    undo_.push_back(values);
    redo_.clear();
}
void ProcessingEdits::beginEdit() { if (!gesture_) gesture_ = values_; }
void ProcessingEdits::endEdit() {
    if (!gesture_) return;
    if (*gesture_ != values_) pushUndo(*gesture_);
    gesture_.reset();
    emit historyChanged();
}
void ProcessingEdits::setValue(const QString& key, double value) {
    if (!values_.contains(key)) return;
    auto next = values_;
    next[key] = value;
    if (!checkedProcessingSettings(next) || next == values_) return;
    if (!gesture_) pushUndo(values_);
    values_ = std::move(next);
    emit valuesChanged();
    emit historyChanged();
}
bool ProcessingEdits::applyValues(const QVariantMap& input) {
    const auto checked = checkedProcessingSettings(input);
    if (!checked) return false;
    endEdit();
    if (*checked == values_) return true;
    pushUndo(values_);
    values_ = *checked;
    emit valuesChanged();
    emit historyChanged();
    return true;
}
void ProcessingEdits::reset() { applyValues(defaultProcessingSettings()); }
void ProcessingEdits::resetSession() {
    gesture_.reset();
    undo_.clear(); redo_.clear();
    values_ = defaultProcessingSettings();
    emit valuesChanged();
    emit historyChanged();
}
void ProcessingEdits::undo() {
    endEdit();
    if (undo_.empty()) return;
    redo_.push_back(values_);
    values_ = std::move(undo_.back()); undo_.pop_back();
    emit valuesChanged(); emit historyChanged();
}
void ProcessingEdits::redo() {
    endEdit();
    if (redo_.empty()) return;
    undo_.push_back(values_);
    values_ = std::move(redo_.back()); redo_.pop_back();
    emit valuesChanged(); emit historyChanged();
}
}
