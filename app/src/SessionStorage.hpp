#pragma once
#include "WorkspaceSnapshot.hpp"
#include <QVariantMap>
#include <QString>
#include <functional>
#include <map>

namespace astro::app {
struct SavedSession {
    QVariantMap ui;
    QMap<QString, WorkspaceSnapshot> workspaces;
};
class SessionStorage {
public:
    explicit SessionStorage(QString directory):directory_(std::move(directory)){}
    bool available() const;
    QString savedAt() const;
    void save(const SavedSession&, const std::function<void()>& check={});
    SavedSession load(const std::function<void()>& check={});
    void discard();
private:
    struct Asset {QString file;QByteArray hash;};
    struct ImageAsset {std::weak_ptr<const processing::FloatImage> image;Asset asset;};
    struct PlaneAsset {std::weak_ptr<const std::vector<float>> plane;Asset asset;};
    QString directory_;
    std::map<const processing::FloatImage*,ImageAsset> images_;
    std::map<const std::vector<float>*,PlaneAsset> planes_;
    std::map<qint64,Asset> previews_;
};
}
