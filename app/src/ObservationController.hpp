#pragma once
#include "ObservationAstronomy.hpp"
#include <QObject>
#include <QHash>
#include <QPointer>
#include <QTimer>
#include <QVariantList>
#include <QVariantMap>
#include <QtQml/qqmlregistration.h>
#include <thread>
class QNetworkAccessManager;
class QNetworkReply;
namespace astro::app {
class ObservationController : public QObject {
  Q_OBJECT
  QML_ELEMENT
  Q_PROPERTY(QVariantMap result READ result NOTIFY changed)
  Q_PROPERTY(QVariantMap settings READ settings NOTIFY changed)
  Q_PROPERTY(QVariantList samples READ samples NOTIFY changed)
  Q_PROPERTY(QVariantList sites READ sites NOTIFY sitesChanged)
  Q_PROPERTY(bool hasHiddenPresetSites READ hasHiddenPresetSites NOTIFY sitesChanged)
  Q_PROPERTY(QVariantList searchResults READ searchResults NOTIFY searchChanged)
  Q_PROPERTY(QVariantList world READ world NOTIFY instantChanged)
  Q_PROPERTY(QVariantMap instant READ instant NOTIFY instantChanged)
  Q_PROPERTY(QVariantMap reference READ reference NOTIFY referenceChanged)
  Q_PROPERTY(QString error READ error NOTIFY changed)
  Q_PROPERTY(bool targetUnavailable READ targetUnavailable NOTIFY changed)
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
      QVariantList visibleTargets READ visibleTargets NOTIFY instantChanged)
  Q_PROPERTY(
      QString connectionStatus READ connectionStatus NOTIFY connectionChanged)
  Q_PROPERTY(QVariantMap weather READ weather NOTIFY weatherChanged)
  Q_PROPERTY(
      QVariantList weatherSamples READ weatherSamples NOTIFY weatherChanged)
  Q_PROPERTY(QVariantMap recommendationWeather READ recommendationWeather NOTIFY
                 weatherChanged)
  Q_PROPERTY(QString mapTimeZoneStatus READ mapTimeZoneStatus NOTIFY changed)
  Q_PROPERTY(bool mapTimeZoneBusy READ mapTimeZoneBusy NOTIFY changed)
public:
  explicit ObservationController(QObject *parent = nullptr);
  ~ObservationController() override;
  QVariantMap result() const { return result_; }
  QVariantMap settings() const { return settings_; }
  QVariantList samples() const { return samples_; }
  QVariantList sites() const { return sites_; }
  bool hasHiddenPresetSites() const;
  QVariantList searchResults() const { return searchResults_; }
  QVariantList world() const { return world_; }
  QVariantMap instant() const { return instant_; }
  QVariantMap reference() const { return reference_; }
  QString error() const { return error_; }
  bool targetUnavailable() const { return targetUnavailable_; }
  QString searchStatus() const { return searchStatus_; }
  bool searching() const { return searching_; }
  int catalogCount() const { return catalogCount_; }
  bool locating() const { return locating_; }
  QString locationStatus() const { return locationStatus_; }
  QVariantList cityResults() const { return cityResults_; }
  QVariantList favoriteTargets() const { return favoriteTargets_; }
  bool targetFavorite() const;
  QVariantList recommendations() const { return recommendations_; }
  QVariantList visibleTargets() const;
  QString connectionStatus() const { return connectionStatus_; }
  QVariantMap weather() const;
  QVariantList weatherSamples() const;
  QVariantMap recommendationWeather() const;
  QString mapTimeZoneStatus() const {
    return settings_.value("mapZoneUnresolved").toBool()
               ? (mapZoneStatus_.isEmpty()
                      ? tr("Time zone not determined · UTC until confirmed.")
                      : mapZoneStatus_)
               : QString();
  }
  bool mapTimeZoneBusy() const {
    return settings_.value("mapZoneUnresolved").toBool() && mapZoneBusy_;
  }
  Q_INVOKABLE void refreshWeather(bool force = true);
  Q_INVOKABLE void toggleFavoriteTarget();
  static QString networkFailure(int code, bool interrupted = false);
  Q_INVOKABLE void searchCity(const QString &name);
  Q_INVOKABLE bool selectCity(int index);
  Q_INVOKABLE void search(const QString &query);
  Q_INVOKABLE void searchOnline(const QString &query);
  Q_INVOKABLE bool selectTarget(const QString &id);
  Q_INVOKABLE bool calculate(const QVariantMap &options);
  Q_INVOKABLE bool changeSite(const QVariantMap &options);
  Q_INVOKABLE QVariantMap planningNight(const QVariantMap &options) const;
  Q_INVOKABLE void setFraction(double value);
  Q_INVOKABLE bool setReferenceZone(const QString &source);
  Q_INVOKABLE QVariantMap referenceTimeChoices(const QString &date,
                                               const QString &clock) const;
  Q_INVOKABLE bool setReferenceUtc(qint64 utc);
  Q_INVOKABLE bool showNow();
  Q_INVOKABLE QVariantMap recordingWindow(qint64 notBefore = 0) const;
  Q_INVOKABLE void chooseMapLocation(double latitude, double longitude);
  Q_INVOKABLE void resolveMapTimeZone();
  Q_INVOKABLE bool saveSite(const QString &name, const QString &website = QString());
  Q_INVOKABLE void restorePresetSites();
  Q_INVOKABLE void toggleFavoriteSite(int index);
  Q_INVOKABLE void removeSite(int index);
signals:
  void changed();
  void instantChanged();
  void referenceChanged();
  void searchChanged();
  void sitesChanged();
  void favoritesChanged();
  void connectionChanged();
  void weatherChanged();

private slots:
  void updateReachability(int state);
  void fetchMapTimeZone();

private:
  void loadCatalog();
  void updateInstant();
  bool calculateAtUtc(const QVariantMap &options, qint64 utc);
  void updateReference();
  void persist();
  void persistSites();
  void confirmMapSites();
  QString localTime(qint64 utc) const;
  void updateRecommendations();
  void saveFavoriteTargets();
  void updateWeatherContext();
  bool weatherPeriodSupported() const;
  ObservationTarget target_;
  bool targetUnavailable_{};
  ObservationSite site_;
  std::vector<ObservationTarget> catalog_;
  struct SearchKeys {
    QString name, designation, id, firstName;
  };
  QHash<QString, SearchKeys> searchKeys_;
  QVariantMap result_, settings_, instant_;
  QVariantMap reference_{{"recording", QVariantMap{{"available", false}}},
                         {"days", QVariantList{}},
                         {"slots", QVariantList{}}};
  QVariantList samples_, sites_, presetSites_, searchResults_, world_;
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
  QVariantMap weatherForecast_;
  QString weatherStatus_, weatherContext_;
  bool weatherBusy_{}, weatherEnabled_{}, weatherFailed_{};
  int weatherGeneration_{};
  QTimer weatherDebounce_, weatherAgeTimer_;
  QNetworkAccessManager *weatherNetwork_{};
  QPointer<QNetworkReply> weatherReply_;
  QTimer mapZoneDebounce_;
  QPointer<QNetworkReply> mapZoneReply_;
  QString mapZoneStatus_;
  bool mapZoneBusy_{};
  int mapZoneGeneration_{};
};
} // namespace astro::app
