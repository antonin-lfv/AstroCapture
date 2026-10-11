#pragma once
#include "ObservationAstronomy.hpp"
#include <QObject>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <thread>
namespace astro::app {
class ObservationController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
  Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
  Q_PROPERTY(QVariantList samples READ samples NOTIFY changed)
  Q_PROPERTY(QVariantList sites READ sites NOTIFY sitesChanged)
  Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
  Q_PROPERTY(QVariantList world READ world NOTIFY instantChanged)
  Q_PROPERTY(QVariantMap instant READ instant NOTIFY instantChanged)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(QString searchStatus READ searchStatus NOTIFY searchChanged)
  Q_PROPERTY(bool searching READ searching NOTIFY searchChanged)
  Q_PROPERTY(int catalogCount READ catalogCount CONSTANT)
  Q_PROPERTY(bool locating READ locating NOTIFY sitesChanged)
  Q_PROPERTY(QString locationStatus READ locationStatus NOTIFY sitesChanged)
  Q_PROPERTY(QVariantList cityResults READ cityResults NOTIFY sitesChanged)
  Q_PROPERTY(
      QVariantList favoriteTargets READ favoriteTargets NOTIFY favoritesChanged)
  Q_PROPERTY(bool targetFavorite READ targetFavorite NOTIFY favoritesChanged)
  Q_PROPERTY(QVariantList recommendations READ recommendations NOTIFY changed)
  Q_PROPERTY(
      QString connectionStatus READ connectionStatus NOTIFY connectionChanged)
public:
  explicit ObservationController(QObject *parent = nullptr);
  ~ObservationController() override;
  QVariantMap result() const { return result_; }
  QVariantMap settings() const { return settings_; }
  QVariantList samples() const { return samples_; }
  QVariantList sites() const { return sites_; }
  QVariantList searchResults() const { return searchResults_; }
  QVariantList world() const { return world_; }
  QVariantMap instant() const { return instant_; }
  QString error() const { return error_; }
  QString searchStatus() const { return searchStatus_; }
  bool searching() const { return searching_; }
  int catalogCount() const { return catalogCount_; }
  bool locating() const { return locating_; }
  QString locationStatus() const { return locationStatus_; }
  QVariantList cityResults() const { return cityResults_; }
  QVariantList favoriteTargets() const { return favoriteTargets_; }
  bool targetFavorite() const;
  QVariantList recommendations() const { return recommendations_; }
  QString connectionStatus() const { return connectionStatus_; }
  Q_INVOKABLE void toggleFavoriteTarget();
  static QString networkFailure(int code, bool interrupted = false);
  Q_INVOKABLE void searchCity(const QString &name);
  Q_INVOKABLE bool selectCity(int index);
  Q_INVOKABLE void search(const QString &query);
  Q_INVOKABLE void searchOnline(const QString &query);
  Q_INVOKABLE bool selectTarget(const QString &id);
  Q_INVOKABLE bool calculate(const QVariantMap &options);
  Q_INVOKABLE void setFraction(double value);
  Q_INVOKABLE void chooseMapLocation(double latitude, double longitude);
  Q_INVOKABLE bool saveSite(const QString &name);
  Q_INVOKABLE void removeSite(int index);
signals:
  void changed();
  void instantChanged();
  void searchChanged();
  void sitesChanged();
  void favoritesChanged();
  void connectionChanged();

private slots:
  void updateReachability(int state);

private:
  void loadCatalog();
  void updateInstant();
  void persist();
  QString localTime(qint64 utc) const;
  void updateRecommendations();
  void saveFavoriteTargets();
  ObservationTarget target_;
  ObservationSite site_;
  std::vector<ObservationTarget> catalog_;
  QVariantMap result_, settings_, instant_;
  QVariantList samples_, sites_, searchResults_, world_;
  QString error_, searchStatus_;
  bool searching_{};
  qint64 start_{}, end_{};
  double fraction_{.5};
  std::jthread resolver_;
  int searchGeneration_{};
  bool locating_{};
  QString locationStatus_;
  QVariantList cityResults_;
  QVariantList favoriteTargets_, recommendations_;
  QString recommendationsKey_, connectionStatus_;
  bool offline_{}, connectionLost_{};
  int reachability_{}, recommendationPool_{}, catalogCount_{};
};
} // namespace astro::app
