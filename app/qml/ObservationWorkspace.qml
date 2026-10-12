import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

Rectangle {
    id: root
    objectName: "observationWorkspace"
    color: Atelier.canvas
    required property var controller
    property bool liveNow: true
    property bool autoSelectVisible: true
    property bool selectingVisible: false
    readonly property var sortedSites: controller.sites.map((value, index) => Object.assign({}, value, {
            sourceIndex: index
        })).sort((a, b) => Number(b.favorite) - Number(a.favorite) || a.name.localeCompare(b.name, Qt.locale().name.replace("_", "-"), {
            sensitivity: "base"
        }))
    function updateVisibleTarget() {
        if (!ready || !autoSelectVisible || selectingVisible || !visible)
            return;
        const targets = filteredSuggestions;
        if (targets.length && targets[0].id !== controller.settings.target) {
            selectingVisible = true;
            manual = false;
            controller.selectTarget(targets[0].id);
            selectingVisible = false;
        }
    }
    function chooseSite(latitude, longitude) {
        autoSelectVisible = true;
        manual = false;
        controller.chooseMapLocation(latitude, longitude);
        refreshSiteWeather();
    }
    readonly property var plotColors: ({
            text: Atelier.text,
            secondary: Atelier.secondary,
            border: Atelier.border,
            accent: Atelier.accent,
            surface: Atelier.surface,
            image: Atelier.image,
            primary: Atelier.primary,
            warning: Atelier.warning
        })
    function chooseFraction(fraction) {
        timeSlider.value = fraction;
        scheduleTimeUpdate();
    }
    function goNow() {
        timeUpdates.stop();
        timeUpdatePending = false;
        referenceNotice = "";
        referenceChoices = [];
        dateNotice = "";
        liveNow = controller.showNow();
    }
    function nextSolar(kind) {
        return (controller.result.siteSolarEvents || []).find(event => event.kind === kind && event.utc > controller.instant.utcSeconds);
    }
    function pcSolar(kind) {
        const event = nextSolar(kind);
        return event ? Qt.formatDateTime(new Date(Number(event.utc) * 1000), "dd/MM HH:mm") : qsTr("none in the next day");
    }
    function refreshSiteWeather() {
        if (autoLoadWeather) {
            weatherRefresh.restart();
            if (controller.settings.mapZoneUnresolved)
                controller.resolveMapTimeZone();
        }
    }
    function prepareNight() {
        goNow();
        liveNow = false;
        const now = controller.instant.utcSeconds;
        const dark = controller.instant.sun <= -18;
        const upcoming = nextSolar("nightStart");
        // Copy the UTC value before changing the period and its event model.
        const upcomingUtc = upcoming ? Number(upcoming.utc) : 0;
        currentNight();
        if (!dark && upcomingUtc) {
            if (upcomingUtc > controller.result.endUtcSeconds)
                stepDay(1);
            controller.setReferenceUtc(upcomingUtc);
        } else if (now >= controller.result.startUtcSeconds && now <= controller.result.endUtcSeconds) {
            controller.setReferenceUtc(now);
        }
        suggestionPeriod = "night";
        mainScroll.contentItem.contentY = 0;
    }
    property string suggestionPeriod: "instant"
    onSuggestionPeriodChanged: Qt.callLater(root.updateVisibleTarget)
    property bool autoLoadWeather: true
    Timer {
        id: weatherRefresh
        interval: 600
        onTriggered: if (root.autoLoadWeather)
            root.controller.refreshWeather(false)
    }
    readonly property var filteredSuggestions: suggestionPeriod === "instant" ? controller.visibleTargets : controller.recommendations
    property bool syncing: false
    property bool ready: false
    property bool timeUpdatePending: false
    property bool manual: false
    property bool citySearchOpen: false
    property bool saveSiteOpen: false
    property string dateNotice: ""
    property string referenceNotice: ""
    property var referenceChoices: []
    property bool referenceClockEdited: false
    function syncReference() {
        const ref = controller.reference;
        referencePicker.currentIndex = ref.source === "site" ? 1 : 0;
        if (!timeSlider.pressed && !timeUpdatePending)
            timeSlider.value = controller.instant.fraction || 0;
        if (!referenceClock.editing && !referenceNotice.length && !referenceChoices.length) {
            referenceClock.text = ref.clock || "";
            referenceDay.currentIndex = (ref.days || []).findIndex(day => day.date === ref.date);
            referenceClockEdited = false;
        }
    }
    function applyReferenceTime() {
        const day = (controller.reference.days || [])[referenceDay.currentIndex];
        if (!day)
            return;
        if (!referenceClockEdited && !referenceNotice.length && !referenceChoices.length && day.date === controller.reference.date && referenceClock.text.trim() === controller.reference.clock)
            return;
        liveNow = false;
        timeUpdates.stop();
        timeUpdatePending = false;
        const answer = controller.referenceTimeChoices(day.date, referenceClock.text);
        referenceChoices = answer.choices || [];
        referenceNotice = answer.error || "";
        if (referenceChoices.length === 1) {
            controller.setReferenceUtc(referenceChoices[0].utc);
            referenceChoices = [];
            referenceClockEdited = false;
            referenceClock.text = controller.reference.clock;
        } else if (referenceChoices.length > 1) {
            referenceNotice = qsTr("This hour occurs twice. Choose the UTC offset below.");
        }
    }
    function showRecordingWindow() {
        const plan = controller.recordingWindow();
        if (plan.available) {
            liveNow = false;
            timeUpdates.stop();
            timeUpdatePending = false;
            referenceNotice = "";
            referenceChoices = [];
            controller.setReferenceUtc(plan.startUtc);
            timeSlider.value = controller.instant.fraction;
        }
    }
    property var nightState: ({})
    function cloudPercent(value) {
        return value === undefined || value === null ? "—" : Math.round(Number(value)) + " %";
    }
    function favoriteSiteIndex() {
        const current = controller.settings;
        for (let i = 0; i < sortedSites.length; i++) {
            const site = sortedSites[i];
            if (site.favorite && Number(site.latitude) === Number(current.latitude) && Number(site.longitude) === Number(current.longitude) && Number(site.elevation) === Number(current.elevation) && site.zone === current.zone)
                return site.sourceIndex;
        }
        return -1;
    }
    function compassDirection(degrees) {
        const directions = [qsTr("North"), qsTr("North-east"), qsTr("East"), qsTr("South-east"), qsTr("South"), qsTr("South-west"), qsTr("West"), qsTr("North-west")];
        return directions[Math.round(Number(degrees) / 45) % 8];
    }
    function checkNight() {
        nightState = controller.planningNight(controller.settings);
    }
    function scheduleTimeUpdate() {
        liveNow = false;
        referenceNotice = "";
        referenceChoices = [];
        if (!timeUpdates.running) {
            controller.setFraction(timeSlider.value);
            timeUpdates.start();
        } else {
            timeUpdatePending = true;
        }
    }
    function finishTimeUpdate() {
        timeUpdates.stop();
        timeUpdatePending = false;
        controller.setFraction(timeSlider.value);
    }
    function applyOptions(options) {
        const current = controller.settings;
        const moved = Number(options.latitude) !== Number(current.latitude) || Number(options.longitude) !== Number(current.longitude) || Number(options.elevation) !== Number(current.elevation) || options.zone !== current.zone;
        if (moved)
            autoSelectVisible = true;
        if (moved && options.date === current.date) {
            dateNotice = "";
            const applied = controller.changeSite(options);
            if (applied)
                refreshSiteWeather();
            return applied;
        }
        if (options.date !== current.date)
            liveNow = false;
        const state = controller.planningNight(options);
        if (state.error) {
            dateNotice = state.error;
            dateInput.text = controller.settings.date;
            return false;
        }
        if (state.past && !history.checked) {
            dateNotice = qsTr("The night of %1 has already ended at this place. Choose the current night or a later date.").arg(options.date);
            dateInput.text = controller.settings.date;
            return false;
        }
        dateNotice = "";
        const applied = controller.calculate(options);
        if (applied)
            refreshSiteWeather();
        return applied;
    }
    function restoreSiteFields() {
        latitude.text = String(controller.settings.latitude);
        longitude.text = String(controller.settings.longitude);
        elevation.text = String(controller.settings.elevation);
        timezone.text = controller.settings.zone;
    }
    function sync() {
        if (controller.error.length)
            return;
        syncing = true;
        const s = controller.settings;
        if (!siteDialog.visible)
            restoreSiteFields();
        if (!dateInput.editing)
            dateInput.text = s.date;
        minimum.value = s.minimum;
        sitePicker.currentIndex = -1;
        for (let i = 0; i < sortedSites.length; i++) {
            const site = sortedSites[i];
            if (site.name === s.name && Number(site.latitude) === Number(s.latitude) && Number(site.longitude) === Number(s.longitude)) {
                sitePicker.currentIndex = i;
                break;
            }
        }
        darkness.currentIndex = [-18, -12, -6, 90].indexOf(Number(s.sunLimit));
        rightAscension.text = String(s.ra || 0);
        declination.text = String(s.dec || 0);
        if (!timeSlider.pressed && !timeUpdatePending)
            timeSlider.value = controller.instant.fraction || 0;
        syncing = false;
        checkNight();
        syncReference();
    }
    function calculate() {
        if (syncing)
            return;
        let s = Object.assign({}, controller.settings, {
            mapZoneUnresolved: false,
            latitude: latitude.text,
            longitude: longitude.text,
            elevation: elevation.text,
            zone: timezone.text,
            date: dateInput.text,
            minimum: minimum.value,
            sunLimit: [-18, -12, -6, 90][darkness.currentIndex],
            manual: manual,
            ra: rightAscension.text,
            dec: declination.text
        });
        if (Number(s.latitude) !== Number(controller.settings.latitude) || Number(s.longitude) !== Number(controller.settings.longitude))
            s.name = qsTr("Custom location");
        return applyOptions(s);
    }
    function shiftedDate(days) {
        let date = new Date(controller.settings.date + "T12:00:00Z");
        if (isNaN(date.getTime()))
            return "";
        date.setUTCDate(date.getUTCDate() + days);
        return date.toISOString().slice(0, 10);
    }
    function stepDay(days) {
        liveNow = false;
        dateInput.text = shiftedDate(days);
        calculate();
    }
    function currentNight() {
        liveNow = false;
        history.checked = false;
        dateInput.text = controller.planningNight(controller.settings).currentDate;
        calculate();
    }
    function chooseTarget(id) {
        autoSelectVisible = false;
        manual = false;
        if (controller.selectTarget(id)) {
            objectChooser.close();
            return true;
        }
        return false;
    }
    function openObjectChooser() {
        query.text = "";
        objectChooser.favouritesOnly = false;
        controller.search("");
        objectChooser.open();
    }
    Connections {
        target: root.controller
        function onChanged() {
            root.referenceNotice = "";
            root.referenceChoices = [];
            root.sync();
        }
        function onInstantChanged() {
            Qt.callLater(root.updateVisibleTarget);
        }
        function onSitesChanged() {
            root.sync();
        }
        function onReferenceChanged() {
            root.syncReference();
        }
    }
    Component.onCompleted: {
        sync();
        ready = true;
        Qt.callLater(root.updateVisibleTarget);
        if (visible && liveNow)
            goNow();
        if (visible)
            refreshSiteWeather();
    }
    onVisibleChanged: if (ready && visible) {
        checkNight();
        if (liveNow)
            goNow();
        refreshSiteWeather();
        Qt.callLater(root.updateVisibleTarget);
    }

    Timer {
        objectName: "observationLiveClock"
        interval: 30000
        running: root.visible && root.liveNow
        repeat: true
        onTriggered: root.goNow()
    }
    Timer {
        interval: 60000
        running: root.visible
        repeat: true
        onTriggered: root.checkNight()
    }
    Timer {
        id: timeUpdates
        interval: 32
        onTriggered: {
            if (root.timeUpdatePending) {
                root.timeUpdatePending = false;
                root.controller.setFraction(timeSlider.value);
                restart();
            }
        }
    }
    ColumnLayout {
        anchors.fill: parent
        spacing: 0
        RowLayout {
            Layout.fillWidth: true
            Layout.margins: 20
            spacing: 16
            Label {
                text: qsTr("Prepare an observation")
                color: Atelier.text
                font.pixelSize: 20
                font.weight: Font.Medium
                Layout.fillWidth: true
            }
            HelpIcon {
                text: qsTr("Choose a place on the day/night map or in the list, then a time and a visible object. Both maps, the compass and the curves follow the displayed instant. Coordinates, favourites and advanced settings open in dialogs. Now follows the real clock.")
            }
        }
        RowLayout {
            objectName: "observationSharedTime"
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 12
            spacing: 16
            Label {
                objectName: "observationSharedPcClock"
                text: qsTr("At home · %1").arg(root.controller.reference.homeLocalDisplay || "")
                color: Atelier.accent
                font.pixelSize: 14
                font.weight: Font.Medium
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
            }
            Label {
                objectName: "observationSharedSiteClock"
                text: (root.controller.settings.mapZoneUnresolved ? qsTr("At the place · UTC until confirmed · %1") : qsTr("At the place · %1")).arg(root.controller.reference.siteLocalDisplay || "")
                color: Atelier.secondary
                font.pixelSize: 13
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 1
            }
            HelpIcon {
                text: qsTr("Both clocks show the selected instant. At home uses this Mac's time zone (%1); at the place uses %2. Clicking a curve or moving the time slider updates both clocks. Now returns to the real current time.").arg(root.controller.reference.homeZone || "").arg(root.controller.reference.siteZone || "")
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 12
            visible: text.length > 0
            text: root.controller.connectionStatus.length > 0 ? root.controller.connectionStatus : root.controller.weather.failed ? root.controller.weather.status : ""
            color: Atelier.warning
            wrapMode: Text.Wrap
            font.pixelSize: 12
            Accessible.role: Accessible.AlertMessage
        }
        ScrollView {
            id: mainScroll
            objectName: "observationScroll"
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: mainScroll.availableWidth
                spacing: 0
                ColumnLayout {
                    id: overviewContent
                    Layout.fillWidth: true
                    Layout.margins: 16
                    spacing: 16
                    RowLayout {
                        id: siteRow
                        objectName: "observationSiteRow"
                        Layout.fillWidth: true
                        spacing: 16
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: (siteRow.width - siteRow.spacing) * .25
                            Layout.alignment: Qt.AlignTop
                            spacing: 16
                            ObservationCard {
                                id: siteCard
                                objectName: "observationSiteSummary"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                title: qsTr("Observing place")
                                help: qsTr("Choose a saved place or click the map. Favourites appear first, with alphabetical sorting in each group. The site and Mac clocks both show the selected instant in their local time zones. Sunrise and sunset are shown in this Mac's time zone. Clouds are forecasts.")
                                DarkComboBox {
                                    id: sitePicker
                                    objectName: "observationSite"
                                    Layout.fillWidth: true
                                    model: root.sortedSites
                                    textRole: "label"
                                    displayText: root.controller.settings.name || qsTr("Custom location")
                                    explanation: qsTr("The city's time zone follows local daylight-saving rules. Changing place keeps the UTC instant; local times and the night date are converted. A map point resolves its local time zone online; if unavailable, Coordinates lets you set it.")
                                    onActivated: index => {
                                        root.manual = false;
                                        let s = Object.assign({}, root.controller.settings, root.sortedSites[index], {
                                            mapZoneUnresolved: root.sortedSites[index].mapZoneUnresolved === true
                                        });
                                        delete s.sourceIndex;
                                        root.applyOptions(s);
                                    }
                                }
                                Label {
                                    objectName: "observationSiteClock"
                                    text: (root.controller.settings.mapZoneUnresolved ? qsTr("At the place · UTC until confirmed\n%1") : qsTr("At the place · selected time\n%1")).arg(root.controller.reference.siteLocalDisplay || "")
                                    color: Atelier.text
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "observationPcClock"
                                    text: qsTr("This Mac · selected time\n%1").arg(root.controller.reference.homeLocalDisplay || "")
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "observationSitePhase"
                                    text: root.controller.instant.solarPhase || ""
                                    color: Atelier.accent
                                    font.pixelSize: 14
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "observationSunset"
                                    text: qsTr("Next sunset · Mac\n%1").arg(root.pcSolar("sunset"))
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    objectName: "observationSunrise"
                                    text: qsTr("Next sunrise · Mac\n%1").arg(root.pcSolar("sunrise"))
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        objectName: "observationSiteClouds"
                                        text: root.controller.weather.busy ? qsTr("Loading…") : qsTr("Clouds: %1").arg(root.cloudPercent(root.controller.weather.cloud_cover))
                                        color: root.controller.weather.stale || root.controller.weather.failed ? Atelier.warning : Atelier.text
                                        font.pixelSize: 13
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                    IconButton {
                                        text: qsTr("Refresh")
                                        busy: root.controller.weather.busy
                                        enabled: root.controller.weather.canRefresh
                                        Accessible.name: qsTr("Refresh the cloud forecast for this site")
                                        onClicked: {
                                            root.controller.refreshWeather();
                                            if (root.controller.settings.mapZoneUnresolved)
                                                root.controller.resolveMapTimeZone();
                                        }
                                    }
                                }
                                Label {
                                    visible: !root.controller.weather.available || root.controller.weather.stale || root.controller.weather.failed
                                    text: root.controller.weather.failed || root.controller.weather.stale ? root.controller.weather.status : qsTr("No cloud forecast for this time.")
                                    color: root.controller.weather.failed || root.controller.weather.stale ? Atelier.warning : Atelier.secondary
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    visible: root.controller.mapTimeZoneStatus.length > 0
                                    text: root.controller.mapTimeZoneStatus
                                    color: Atelier.warning
                                    font.pixelSize: 11
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                IconButton {
                                    Layout.fillWidth: true
                                    enabled: !root.controller.mapTimeZoneBusy
                                    text: root.favoriteSiteIndex() >= 0 ? qsTr("Rename…") : qsTr("Save place…")
                                    objectName: "observationNamePlace"
                                    Accessible.name: root.favoriteSiteIndex() >= 0 ? qsTr("Rename this saved place") : qsTr("Name and save this place")
                                    onClicked: {
                                        root.saveSiteOpen = true;
                                        nameDialog.open();
                                        favoriteName.text = root.controller.settings.name || "";
                                    }
                                }
                                GridLayout {
                                    Layout.fillWidth: true
                                    columns: siteCard.width >= 290 ? 2 : 1
                                    columnSpacing: 8
                                    rowSpacing: 8
                                    IconButton {
                                        text: qsTr("City…")
                                        Layout.fillWidth: true
                                        onClicked: {
                                            root.citySearchOpen = true;
                                            cityDialog.open();
                                        }
                                    }
                                    IconButton {
                                        objectName: "observationCoordinates"
                                        text: qsTr("Coordinates…")
                                        Layout.fillWidth: true
                                        onClicked: {
                                            root.sync();
                                            siteDialog.open();
                                        }
                                    }
                                }
                            }
                            ObservationCard {
                                id: timeCard
                                objectName: "observationTimeCard"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                title: qsTr("Choose the time")
                                IconButton {
                                    objectName: "observationNow"
                                    Layout.fillWidth: true
                                    text: root.liveNow ? qsTr("Now · live") : qsTr("Now")
                                    borderColor: root.liveNow ? Atelier.accent : Atelier.border
                                    onClicked: root.goNow()
                                    Accessible.description: root.liveNow ? qsTr("Live time is active") : qsTr("Return to the current time")
                                }
                                ObservationField {
                                    id: referenceClock
                                    objectName: "observationReferenceClock"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    label: root.controller.reference.source === "site" ? qsTr("Hour at the place · HH:mm") : qsTr("Hour at home · HH:mm")
                                    help: qsTr("Enter the time in the selected reference time zone. The cursor, both maps and all curves move to the same instant. For example, 23:30. Clock-change gaps are rejected; repeated hours let you choose the offset.")
                                    onAccepted: root.applyReferenceTime()
                                    onTextEdited: {
                                        root.liveNow = false;
                                        root.referenceClockEdited = true;
                                    }
                                }
                                Label {
                                    objectName: "observationReferenceReadout"
                                    text: (root.controller.reference.source === "site" ? qsTr("Telescope: %1") : qsTr("At home: %1")).arg(root.controller.reference.source === "site" ? root.controller.reference.siteLocalDisplay || "" : root.controller.reference.homeLocalDisplay || "")
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                                AtelierSlider {
                                    id: timeSlider
                                    objectName: "observationTime"
                                    Layout.fillWidth: true
                                    from: 0
                                    to: 1
                                    value: .5
                                    stepSize: 1 / 1440
                                    live: true
                                    Accessible.name: qsTr("Explore the night")
                                    onMoved: root.scheduleTimeUpdate()
                                    onPressedChanged: if (!pressed && root.ready)
                                        root.finishTimeUpdate()
                                }

                                IconButton {
                                    objectName: "observationTimeOptions"
                                    Layout.fillWidth: true
                                    text: qsTr("Date and time zone…")
                                    onClicked: timeDialog.open()
                                }
                                IconButton {
                                    objectName: "observationPrepareNight"
                                    Layout.fillWidth: true
                                    text: qsTr("Prepare this night")
                                    onClicked: root.prepareNight()
                                }
                                Label {
                                    objectName: "observationDateNotice"
                                    visible: root.dateNotice.length > 0 || (root.nightState.past && !history.checked && !root.liveNow)
                                    text: root.dateNotice.length ? root.dateNotice : qsTr("This night has ended. Choose Current night to continue planning.")
                                    color: Atelier.warning
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 12
                                }
                                Label {
                                    visible: root.referenceNotice.length > 0
                                    text: root.referenceNotice
                                    color: Atelier.warning
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    font.pixelSize: 12
                                }
                                DarkComboBox {
                                    visible: root.referenceChoices.length > 1
                                    objectName: "observationReferenceOccurrence"
                                    Layout.fillWidth: true
                                    model: root.referenceChoices
                                    textRole: "label"
                                    displayText: qsTr("Choose the occurrence of this hour")
                                    Accessible.name: qsTr("Choose the UTC offset for the repeated hour")
                                    onActivated: index => {
                                        root.liveNow = false;
                                        root.controller.setReferenceUtc(root.referenceChoices[index].utc);
                                        root.referenceChoices = [];
                                        root.referenceNotice = "";
                                        root.referenceClockEdited = false;
                                        referenceClock.text = root.controller.reference.clock;
                                    }
                                }
                            }
                            ObservationCard {
                                objectName: "observationSkyCard"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                title: qsTr("Where to look?")
                                help: qsTr("A compass view of the sky around your place: north at the top, east on the right. The edge is the theoretical horizon (0°); the centre is overhead, the zenith (90°). The point shows the selected object at the cursor time; the line is its path over the night. Use the cardinal direction and height to know where to look. This is a diagram, not a globe, photograph or the actual terrain horizon.")
                                Label {
                                    text: qsTr("%1 · %2° above the horizon").arg(root.compassDirection(root.controller.instant.azimuth)).arg(Number(root.controller.instant.altitude || 0).toFixed(1))
                                    color: Atelier.accent
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                ObservationPlot {
                                    objectName: "observationSkyPlot"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(210, width + 20)
                                    controller: root.controller
                                    mode: "sky"
                                    colors: root.plotColors
                                    Accessible.role: Accessible.Graphic
                                    Accessible.name: qsTr("Object's trajectory above the horizon")
                                }
                                Label {
                                    text: root.controller.instant.altitude < 0 ? qsTr("The object is below the horizon at this time.") : qsTr("Edge: horizon · centre: overhead")
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                            }
                        }
                        ColumnLayout {
                            Layout.fillWidth: true
                            Layout.alignment: Qt.AlignTop
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: (siteRow.width - siteRow.spacing) * .75
                            spacing: 16
                            ObservationCard {
                                objectName: "observationDayCard"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                title: qsTr("Day and night around the world")
                                help: qsTr("Day follows the Sun's upper edge with standard refraction at sea level; astronomical night starts below −18°. The maps use a regional 6° grid and the same displayed instant. Clicking selects a place without changing UTC. Its time zone is resolved online; if unavailable, Coordinates lets you set it.")

                                Label {
                                    objectName: "observationMapTime"
                                    text: root.controller.instant.utc || ""
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                                ObservationPlot {
                                    objectName: "observationDayPlot"
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: Math.max(240, (width - 32) / 2 + 40)
                                    controller: root.controller
                                    mode: "daylight"
                                    HoverHandler {
                                        cursorShape: Qt.CrossCursor
                                    }
                                    colors: root.plotColors
                                    Accessible.role: Accessible.Graphic
                                    Accessible.name: qsTr("World day, twilight and night map at the shared UTC instant")
                                    onLocationChosen: (latitude, longitude) => root.chooseSite(latitude, longitude)
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Repeater {
                                        model: [
                                            {
                                                color: Atelier.warning,
                                                label: qsTr("Day")
                                            },
                                            {
                                                color: Atelier.primary,
                                                label: qsTr("Twilight")
                                            },
                                            {
                                                color: Atelier.image,
                                                label: qsTr("Night")
                                            }
                                        ]
                                        Row {
                                            required property var modelData
                                            spacing: 6
                                            Rectangle {
                                                width: 10
                                                height: 10
                                                anchors.verticalCenter: parent.verticalCenter
                                                color: modelData.color
                                                border.color: Atelier.border
                                                radius: 2
                                            }
                                            Label {
                                                text: modelData.label
                                                color: Atelier.secondary
                                                font.pixelSize: 12
                                            }
                                        }
                                    }
                                }
                            }
                            ObservationCard {
                                id: objectCard
                                objectName: "observationObjectCard"
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                title: qsTr("Choose an object")
                                GridLayout {
                                    columns: objectCard.width >= 560 ? 2 : 1
                                    columnSpacing: 16
                                    rowSpacing: 12
                                    Layout.fillWidth: true
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredWidth: 1
                                        spacing: 5
                                        SettingLabel {
                                            text: qsTr("Recommendations")
                                            Layout.fillWidth: true
                                        }
                                        DarkComboBox {
                                            objectName: "observationSuggestionPeriod"
                                            Layout.fillWidth: true
                                            model: [qsTr("At the displayed time"), qsTr("Over this night")]
                                            Accessible.name: qsTr("Recommendation period")
                                            currentIndex: root.suggestionPeriod === "instant" ? 0 : 1
                                            onActivated: index => {
                                                root.autoSelectVisible = true;
                                                if (index === 1 && root.liveNow)
                                                    root.prepareNight();
                                                else
                                                    root.suggestionPeriod = index === 0 ? "instant" : "night";
                                                root.updateVisibleTarget();
                                            }
                                        }
                                    }
                                    ColumnLayout {
                                        Layout.fillWidth: true
                                        Layout.minimumWidth: 0
                                        Layout.preferredWidth: 1
                                        spacing: 5
                                        SettingLabel {
                                            text: qsTr("Selected object")
                                            Layout.fillWidth: true
                                        }
                                        DarkComboBox {
                                            objectName: "observationVisibleTarget"
                                            Layout.fillWidth: true
                                            Layout.minimumWidth: 0
                                            model: root.filteredSuggestions
                                            textRole: "name"
                                            Accessible.name: qsTr("Selected object")
                                            enabled: root.filteredSuggestions.length > 0
                                            currentIndex: root.filteredSuggestions.findIndex(value => value.id === root.controller.settings.target)
                                            displayText: root.filteredSuggestions.length ? root.controller.result.target || "" : root.suggestionPeriod === "instant" ? qsTr("Nothing visible at this time") : qsTr("Nothing visible during this night")
                                            onActivated: index => root.chooseTarget(root.filteredSuggestions[index].id)
                                        }
                                    }
                                }
                                RowLayout {
                                    Layout.fillWidth: true
                                    Label {
                                        objectName: "observationSelectedTarget"
                                        readonly property var recommendation: root.filteredSuggestions.find(value => value.id === root.controller.settings.target)
                                        text: root.filteredSuggestions.length ? root.controller.result.target || "" : qsTr("Displayed object: %1").arg(root.controller.result.target || "")
                                        color: Atelier.accent
                                        font.pixelSize: 14
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                    }
                                    IconButton {
                                        objectName: "observationFavoriteTarget"
                                        text: root.controller.targetFavorite ? "★" : "☆"
                                        Layout.preferredWidth: 32
                                        Accessible.name: root.controller.targetFavorite ? qsTr("★ Remove object from favourites") : qsTr("☆ Save object to favourites")
                                        onClicked: root.controller.toggleFavoriteTarget()
                                    }
                                }
                                Label {
                                    readonly property var recommendation: root.filteredSuggestions.find(value => value.id === root.controller.settings.target)
                                    visible: recommendation !== undefined
                                    text: !recommendation ? "" : recommendation.altitude !== undefined ? qsTr("Height now: %1°").arg(Number(recommendation.altitude).toFixed(1)) : qsTr("%1 min available during this night").arg(recommendation.minutes)
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                                Label {
                                    visible: !root.filteredSuggestions.length
                                    text: root.suggestionPeriod === "instant" ? qsTr("No recommendation meets the limits at this time. Prepare the night or choose an object from the catalogue.") : qsTr("No suggestion meets these limits here on this date. Try lowering the minimum height or choosing another night. You can still search the complete catalogue.")
                                    color: Atelier.warning
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Flow {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    IconButton {
                                        objectName: "observationChooseTarget"
                                        text: qsTr("Catalogue and favourites…")
                                        onClicked: root.openObjectChooser()
                                    }
                                    IconButton {
                                        text: qsTr("Visibility settings…")
                                        onClicked: optionsDialog.open()
                                    }
                                }
                            }
                            RowLayout {
                                id: visibilityRow
                                objectName: "observationVisibilityRow"
                                Layout.fillWidth: true
                                spacing: 16
                                ObservationCard {
                                    objectName: "observationWorldCard"
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    Layout.fillHeight: true
                                    title: qsTr("Where is %1 visible?").arg((root.controller.result.target || "").split(" · ")[0])
                                    help: qsTr("Day follows the Sun's upper edge with standard refraction at sea level; astronomical night starts below −18°. The maps use a regional 6° grid and the same displayed instant. Clicking selects a place without changing UTC. Its time zone is resolved online; if unavailable, Coordinates lets you set it.")

                                    Label {
                                        objectName: "observationVisibilityTime"
                                        text: root.controller.instant.utc || ""
                                        color: Atelier.secondary
                                        font.pixelSize: 12
                                        Layout.fillWidth: true
                                        wrapMode: Text.Wrap
                                    }
                                    ObservationPlot {
                                        objectName: "observationWorldPlot"
                                        Layout.fillWidth: true
                                        Layout.preferredHeight: Math.max(240, (width - 32) / 2 + 40)
                                        controller: root.controller
                                        mode: "world"
                                        HoverHandler {
                                            cursorShape: Qt.CrossCursor
                                        }
                                        colors: root.plotColors
                                        Accessible.role: Accessible.Graphic
                                        Accessible.name: qsTr("World visibility map at the selected UTC instant")
                                        onLocationChosen: (latitude, longitude) => root.chooseSite(latitude, longitude)
                                    }
                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        Repeater {
                                            model: [
                                                {
                                                    color: Atelier.primary,
                                                    label: qsTr("Limits met")
                                                },
                                                {
                                                    color: Atelier.warning,
                                                    label: qsTr("Too low")
                                                },
                                                {
                                                    color: Atelier.secondary,
                                                    label: qsTr("Sky too bright")
                                                },
                                                {
                                                    color: Atelier.image,
                                                    label: qsTr("Below horizon")
                                                }
                                            ]
                                            Row {
                                                required property var modelData
                                                spacing: 6
                                                Rectangle {
                                                    width: 10
                                                    height: 10
                                                    anchors.verticalCenter: parent.verticalCenter
                                                    color: modelData.color
                                                    border.color: Atelier.border
                                                    radius: 2
                                                }
                                                Label {
                                                    text: modelData.label
                                                    color: Atelier.secondary
                                                    font.pixelSize: 12
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    RowLayout {
                        id: curvesRow
                        objectName: "observationCurvesRow"
                        Layout.fillWidth: true
                        spacing: 16
                        ObservationCard {
                            objectName: "observationHeightCard"
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: (curvesRow.width - curvesRow.spacing) * .5
                            title: qsTr("Height of %1").arg((root.controller.result.target || "").split(" · ")[0])
                            help: qsTr("Blue intervals meet the selected height and darkness limits. Drag the time slider or click the curve. Terrain and weather are not included in geometric visibility.")
                            Label {
                                text: qsTr("Height: %1°").arg(Number(root.controller.instant.altitude || 0).toFixed(1)) + " · " + (root.controller.instant.status || "")
                                color: Atelier.accent
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                text: qsTr("Chart hours: %1 (telescope)").arg(root.controller.reference.siteZone || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            ObservationPlot {
                                objectName: "observationPlot"
                                Layout.fillWidth: true
                                Layout.preferredHeight: 240
                                controller: root.controller
                                mode: "night"
                                colors: root.plotColors
                                Accessible.role: Accessible.Graphic
                                Accessible.name: qsTr("Height curve over the chosen night, with eligible intervals in blue")
                                onFractionChosen: fraction => root.chooseFraction(fraction)
                            }
                        }
                        ObservationCard {
                            objectName: "observationCloudCard"
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            Layout.preferredWidth: (curvesRow.width - curvesRow.spacing) * .5
                            Layout.fillHeight: true
                            title: qsTr("Clouds above %1").arg(root.controller.settings.name || "")
                            help: qsTr("0% means no clouds predicted in the model grid cell; 100% means complete coverage. Hourly values are forecasts, not live measurements. Cloud cover alone does not predict transparency or atmospheric turbulence.")

                            ColumnLayout {
                                Layout.fillWidth: true
                                spacing: 8
                                RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 12
                                    Label {
                                        text: root.controller.weather.available ? root.cloudPercent(root.controller.weather.cloud_cover) : "—"
                                        color: Atelier.text
                                        font.pixelSize: 28
                                        Layout.fillWidth: true
                                    }
                                    IconButton {
                                        objectName: "observationLoadWeather"
                                        text: root.controller.weather.busy ? qsTr("Loading…") : root.controller.weather.hasForecast ? qsTr("Refresh clouds") : qsTr("Load clouds")
                                        busy: root.controller.weather.busy
                                        enabled: root.controller.weather.canRefresh
                                        Accessible.name: qsTr("Load or refresh the cloud forecast for this place")
                                        onClicked: root.controller.refreshWeather()
                                    }
                                }
                                Label {
                                    visible: root.controller.weather.available
                                    text: qsTr("Forecast instant at the telescope: %1").arg(root.controller.weather.validTime || "")
                                    color: Atelier.secondary
                                    font.pixelSize: 12
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    visible: root.controller.weather.status.length > 0
                                    text: root.controller.weather.status
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: root.controller.weather.failed ? Atelier.warning : Atelier.secondary
                                    font.pixelSize: 12
                                    Accessible.role: root.controller.weather.failed ? Accessible.AlertMessage : Accessible.StaticText
                                }
                                Label {
                                    visible: root.controller.weather.stale && root.controller.weather.hasForecast
                                    text: qsTr("Old forecast · refresh before observing")
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                    color: Atelier.warning
                                    font.pixelSize: 12
                                }
                            }
                            ObservationPlot {
                                objectName: "observationCloudPlot"
                                visible: true
                                Layout.fillWidth: true
                                Layout.preferredHeight: 240
                                controller: root.controller
                                mode: "clouds"
                                colors: root.plotColors
                                Accessible.role: Accessible.Graphic
                                Accessible.name: qsTr("Hourly forecast cloud coverage from 0 to 100 percent over the selected period")
                                onFractionChosen: fraction => root.chooseFraction(fraction)
                            }

                            InspectorFamily {
                                Layout.fillWidth: true
                                objectName: "observationWeatherDetails"
                                title: qsTr("Forecast details & source")
                                onActivated: expanded = !expanded
                                Label {
                                    visible: root.controller.weather.available
                                    text: qsTr("Low: %1 · mid: %2 · high: %3").arg(root.cloudPercent(root.controller.weather.cloud_cover_low)).arg(root.cloudPercent(root.controller.weather.cloud_cover_mid)).arg(root.cloudPercent(root.controller.weather.cloud_cover_high))
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 13
                                }
                                Label {
                                    text: qsTr("Each point is an hourly forecast. The cursor shows the nearest available hour, within 30 minutes. Gaps mean missing data. No cloud percentage is extrapolated outside the returned forecast. Low, mid and high layers overlap and must not be added.")
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                }
                                Label {
                                    visible: root.controller.weather.received !== undefined
                                    text: qsTr("Retrieved: %1 · model cell: %2°, %3°").arg(root.controller.weather.received || "").arg(Number(root.controller.weather.gridLatitude || 0).toFixed(3)).arg(Number(root.controller.weather.gridLongitude || 0).toFixed(3))
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                }
                                Label {
                                    text: qsTr("Source: <a href='https://open-meteo.com/en/docs'>Open-Meteo</a> · <a href='https://creativecommons.org/licenses/by/4.0/'>CC BY 4.0</a>. Only this place's coordinates are sent when loading a forecast.")
                                    textFormat: Text.StyledText
                                    linkColor: Atelier.accent
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                    onLinkActivated: link => Qt.openUrlExternally(link)
                                }
                            }
                            Label {
                                visible: root.controller.weather.hasForecast
                                Layout.fillWidth: true
                                text: qsTr("Forecast: <a href='https://open-meteo.com/'>Open-Meteo</a> · <a href='https://creativecommons.org/licenses/by/4.0/'>CC BY 4.0</a>")
                                textFormat: Text.StyledText
                                linkColor: Atelier.accent
                                color: Atelier.secondary
                                wrapMode: Text.Wrap
                                font.pixelSize: 12
                                onLinkActivated: link => Qt.openUrlExternally(link)
                            }
                        }
                    }
                    Label {
                        visible: root.controller.error.length > 0
                        text: root.controller.error + "\n" + qsTr("The displayed results keep the previous valid choices.")
                        color: Atelier.danger
                        wrapMode: Text.Wrap
                        Layout.fillWidth: true
                    }
                    ColumnLayout {
                        objectName: "observationWindows"
                        Layout.fillWidth: true
                        spacing: 8
                        SettingLabel {
                            visible: !(history.checked && root.nightState.past)
                            text: qsTr("When to start recording?")
                            explanation: qsTr("The first remaining window is chosen after the current real time, independently of the exploratory cursor. It meets the selected height and darkness limits. Start is rounded up to a full minute; end is rounded down. The main hours always use this Mac’s local time; telescope hours convert the same instants. Check weather, terrain and telescope setup separately.")
                        }
                        ColumnLayout {
                            visible: !(history.checked && root.nightState.past) && root.controller.reference.recording.available === true
                            Layout.fillWidth: true
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Label {
                                    objectName: "observationRecordingStart"
                                    text: qsTr("At home: from %1").arg(root.controller.reference.recording.homeStartLocal || "")
                                    color: Atelier.text
                                    font.pixelSize: 16
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                IconButton {
                                    objectName: "observationShowRecordingWindow"
                                    text: qsTr("Show start")
                                    onClicked: root.showRecordingWindow()
                                }
                            }
                            Label {
                                objectName: "observationRecordingEnd"
                                text: qsTr("Until %1 · %2 min").arg(root.controller.reference.recording.homeEndLocal || "").arg(root.controller.reference.recording.minutes || 0)
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: root.controller.reference.differentZones
                                objectName: "observationRecordingSite"
                                text: (root.controller.settings.mapZoneUnresolved ? qsTr("At the telescope · UTC until confirmed: %1 → %2") : qsTr("At the telescope: %1 → %2")).arg(root.controller.reference.recording.siteStartLocal || "").arg(root.controller.reference.recording.siteEndLocal || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            visible: !(history.checked && root.nightState.past) && !root.controller.reference.recording.available
                            text: qsTr("No remaining recording window in this night with the selected limits.")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                        InspectorFamily {
                            objectName: "observationAllWindows"
                            visible: true
                            title: qsTr("All windows · Mac local time")
                            onActivated: expanded = !expanded
                            Label {
                                visible: (root.controller.reference.slots || []).length === 0
                                text: qsTr("No window with these limits. Try a different date, location or minimum height.")
                                color: Atelier.secondary
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Repeater {
                                model: root.controller.reference.slots || []
                                ColumnLayout {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    spacing: 4
                                    Label {
                                        text: modelData.homeStartLocal + " → " + modelData.homeEndLocal
                                        color: Atelier.text
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                        font.pixelSize: 13
                                    }
                                    Label {
                                        visible: root.controller.reference.differentZones
                                        text: (root.controller.settings.mapZoneUnresolved ? qsTr("At the telescope · UTC until confirmed: %1 → %2") : qsTr("At the telescope: %1 → %2")).arg(modelData.siteStartLocal).arg(modelData.siteEndLocal)
                                        color: Atelier.secondary
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    DarkDialog {
        id: timeDialog
        objectName: "observationTimeDialog"
        title: qsTr("Date and reference time")
        modal: true
        parent: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(560, root.width - 48)
        height: Math.min(430, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        contentItem: ColumnLayout {
            spacing: 12
            SettingLabel {
                text: qsTr("Reference time zone")
                Layout.fillWidth: true
            }
            DarkComboBox {
                id: referencePicker
                objectName: "observationReferenceZone"
                Layout.fillWidth: true
                model: [qsTr("At home · %1").arg(root.controller.reference.homeZone || ""), qsTr("Telescope · %1").arg(root.controller.reference.siteZone || "")]
                Accessible.name: qsTr("Reference time zone")
                onActivated: index => {
                    root.referenceChoices = [];
                    root.referenceNotice = "";
                    root.controller.setReferenceZone(index === 1 ? "site" : "system");
                }
            }

            SettingLabel {
                text: qsTr("Date in the reference time zone")
                Layout.fillWidth: true
            }
            DarkComboBox {
                id: referenceDay
                objectName: "observationReferenceDay"
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: root.controller.reference.days || []
                textRole: "label"
                Accessible.name: qsTr("Date in the reference time zone")
                explanation: qsTr("Choose the calendar day in the reference time zone. A night can cross midnight and a time-zone conversion can change the date.")
                onActivated: root.applyReferenceTime()
            }

            ObservationField {
                id: dateInput
                objectName: "observationDate"
                Layout.fillWidth: true
                Layout.minimumWidth: 120
                label: qsTr("Local date · YYYY-MM-DD")
                onTextEdited: root.liveNow = false
                help: qsTr("Choose an upcoming night or use Current night. Past dates can be explored in Advanced.")
                onAccepted: root.calculate()
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                IconButton {
                    objectName: "observationPreviousNight"
                    text: "‹"
                    Layout.preferredWidth: 32
                    Accessible.name: qsTr("Previous night")
                    enabled: history.checked || !root.controller.planningNight(Object.assign({}, root.controller.settings, {
                        date: root.shiftedDate(-1)
                    })).past
                    onClicked: root.stepDay(-1)
                }
                IconButton {
                    text: "›"
                    Layout.preferredWidth: 32
                    Accessible.name: qsTr("Next night")
                    onClicked: root.stepDay(1)
                }
                IconButton {
                    Layout.fillWidth: true
                    objectName: "observationCurrentNight"
                    text: qsTr("Current night")
                    onClicked: root.currentNight()
                }
            }

            IconButton {
                Layout.fillWidth: true
                text: qsTr("Done")
                onClicked: timeDialog.close()
            }
        }
    }
    DarkDialog {
        id: siteDialog
        objectName: "observationSiteDialog"
        title: qsTr("Coordinates and time zone")
        modal: true
        parent: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(560, root.width - 48)
        height: Math.min(500, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        onClosed: root.restoreSiteFields()
        contentItem: ScrollView {
            id: siteDialogScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: siteDialogScroll.availableWidth
                spacing: 12
                ObservationField {
                    id: latitude
                    objectName: "observationLatitude"
                    Layout.fillWidth: true
                    label: qsTr("Latitude · degrees")
                    help: qsTr("North is positive; south is negative. Range −90 to +90.")
                }
                ObservationField {
                    id: longitude
                    objectName: "observationLongitude"
                    Layout.fillWidth: true
                    label: qsTr("Longitude · degrees")
                    help: qsTr("East is positive; west is negative. Range −180 to +180.")
                }
                ObservationField {
                    id: elevation
                    objectName: "observationElevation"
                    Layout.fillWidth: true
                    label: qsTr("Elevation · metres")
                    help: qsTr("Height above sea level, −500 to 10000 metres. Sunrise uses standard refraction adjusted for this elevation. Terrain and horizon dip are not modelled.")
                }
                ObservationField {
                    id: timezone
                    objectName: "observationZone"
                    Layout.fillWidth: true
                    label: qsTr("Time zone")
                    help: qsTr("Use an IANA name such as Europe/Paris, America/Santiago or UTC. Clock changes are applied automatically.")
                }
                Label {
                    visible: root.controller.error.length > 0
                    text: root.controller.error
                    color: Atelier.danger
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                IconButton {
                    objectName: "observationApplyCoordinates"
                    text: qsTr("Apply position")
                    Layout.fillWidth: true
                    onClicked: {
                        if (root.calculate())
                            siteDialog.close();
                    }
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Close")
                onClicked: siteDialog.close()
            }
        }
    }
    DarkDialog {
        id: nameDialog
        objectName: "observationNameDialog"
        title: qsTr("Favourite place")
        modal: true
        parent: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(560, root.width - 48)
        height: Math.min(330, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        onOpened: favoriteName.focusInput(true)
        onClosed: root.saveSiteOpen = false
        contentItem: ScrollView {
            id: nameDialogScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: nameDialogScroll.availableWidth
                spacing: 12
                ObservationField {
                    id: favoriteName
                    objectName: "observationPlaceName"

                    Layout.fillWidth: true
                    label: qsTr("Place name")
                    help: qsTr("For example Garden, Balcony or Mountain site. The name, coordinates and time zone are saved on this computer as a favourite, including points chosen on the map. Renaming keeps the same position.")
                }
                Label {
                    text: qsTr("%1°, %2° · %3").arg(root.controller.settings.latitude).arg(root.controller.settings.longitude).arg(root.controller.settings.zone)
                    color: Atelier.secondary
                    Layout.fillWidth: true
                    wrapMode: Text.Wrap
                }
                IconButton {
                    text: qsTr("Save this site")
                    enabled: favoriteName.text.trim().length > 0
                    Layout.fillWidth: true
                    onClicked: if (root.controller.saveSite(favoriteName.text))
                        nameDialog.close()
                }
                IconButton {
                    text: qsTr("Remove favourite")
                    visible: root.favoriteSiteIndex() >= 0
                    Layout.fillWidth: true
                    onClicked: {
                        root.controller.removeSite(root.favoriteSiteIndex());
                        nameDialog.close();
                    }
                }
                Label {
                    text: root.controller.error
                    visible: text.length > 0
                    color: Atelier.danger
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Close")
                onClicked: nameDialog.close()
            }
        }
    }
    DarkDialog {
        id: cityDialog
        objectName: "observationCityDialog"
        title: qsTr("Find a city")
        modal: true
        parent: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(560, root.width - 48)
        height: Math.min(480, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        onOpened: cityQuery.focusInput()
        onClosed: root.citySearchOpen = false
        contentItem: ScrollView {
            id: cityDialogScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: cityDialogScroll.availableWidth
                spacing: 12
                ObservationField {
                    id: cityQuery

                    Layout.fillWidth: true
                    label: qsTr("Another city")
                    help: qsTr("Enter a city name, optionally followed by its country. Searching sends only this text to Open-Meteo / GeoNames.")
                }
                IconButton {

                    Layout.fillWidth: true
                    text: root.controller.locating ? qsTr("Searching…") : qsTr("Find this city online")
                    busy: root.controller.locating
                    enabled: !root.controller.locating && cityQuery.text.trim().length >= 2
                    onClicked: root.controller.searchCity(cityQuery.text)
                }
                Label {
                    text: root.controller.locationStatus
                    visible: text.length > 0
                    color: Atelier.secondary
                    wrapMode: Text.Wrap
                    Layout.fillWidth: true
                }
                Repeater {
                    model: root.citySearchOpen ? root.controller.cityResults : []
                    IconButton {
                        required property int index
                        required property var modelData
                        Layout.fillWidth: true
                        text: modelData.name
                        onClicked: {
                            root.manual = false;
                            if (root.applyOptions(Object.assign({}, root.controller.settings, modelData, {
                                mapZoneUnresolved: false
                            }))) {
                                root.citySearchOpen = false;
                                cityDialog.close();
                            }
                        }
                    }
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Close")
                onClicked: cityDialog.close()
            }
        }
    }
    DarkDialog {
        id: optionsDialog
        objectName: "observationOptionsDialog"
        title: qsTr("Visibility and advanced options")
        modal: true
        parent: Overlay.overlay
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        width: Math.min(560, root.width - 48)
        height: Math.min(560, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        onOpened: {
            criteriaSection.expanded = true;
        }
        contentItem: ScrollView {
            id: optionsDialogScroll
            clip: true
            contentWidth: availableWidth
            ColumnLayout {
                width: optionsDialogScroll.availableWidth
                spacing: 12
                InspectorFamily {
                    id: criteriaSection
                    title: qsTr("Visibility settings")
                    objectName: "observationCriteria"
                    onActivated: expanded = !expanded
                    ParameterControl {
                        id: minimum
                        objectName: "observationMinimum"
                        Layout.fillWidth: true
                        label: qsTr("Minimum height · degrees")
                        explanation: qsTr("30° is a useful starting point. Higher limits reduce the available time but avoid the thickest atmosphere near the horizon. Lowering this limit cannot remove buildings or mountains.")
                        from: 0
                        to: 80
                        value: 30
                        decimals: 0
                        stepSize: 1
                        onGestureEnded: root.calculate()
                    }
                    DarkComboBox {
                        id: darkness
                        objectName: "observationDarkness"
                        Layout.fillWidth: true
                        model: [qsTr("Very dark · Sun below −18°"), qsTr("Dark · Sun below −12°"), qsTr("Twilight · Sun below −6°"), qsTr("Any sky brightness")]
                        explanation: qsTr("Deep-sky objects benefit from a very dark sky. The Moon and bright planets can be observed in twilight, so that limit is suggested automatically. Any sky brightness also includes daytime; exercise particular care with objects near the Sun.")
                        onActivated: root.calculate()
                    }
                }
                InspectorFamily {
                    id: advanced
                    Layout.fillWidth: true
                    title: qsTr("Advanced")
                    expanded: false
                    onActivated: expanded = !expanded
                    ColumnLayout {
                        Layout.fillWidth: true
                        spacing: 12
                        CheckBox {
                            id: history
                            objectName: "observationHistory"
                            text: qsTr("Explore past dates")
                            palette.windowText: Atelier.text
                            Accessible.description: qsTr("Past dates are available for reviewing an observation. Turn this off to plan an upcoming night.")
                            onToggled: {
                                root.dateNotice = "";
                                root.checkNight();
                            }
                        }
                        Label {
                            text: qsTr("Set an exact observing site or J2000 celestial coordinates. The module calculates body centres without atmospheric refraction, terrain or a local horizon mask.")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }

                        ObservationField {
                            id: rightAscension
                            Layout.fillWidth: true
                            label: qsTr("Right ascension J2000 · degrees")
                            help: qsTr("0 to less than 360 degrees. If your catalogue gives hours, multiply by 15.")
                        }
                        ObservationField {
                            id: declination
                            Layout.fillWidth: true
                            label: qsTr("Declination J2000 · degrees")
                            help: qsTr("−90 to +90 degrees. Manual coordinates are for fixed objects, not the Moon or planets.")
                        }
                        IconButton {
                            text: qsTr("Use these celestial coordinates")
                            onClicked: {
                                root.manual = true;
                                root.autoSelectVisible = false;
                                root.calculate();
                            }
                        }
                        Label {
                            text: qsTr("Source: %1").arg(root.controller.result.source || "")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("OpenNGC: %1 objects with coordinates, plus Solar System targets. Positions include precession and nutation. Fixed objects omit proper motion and annual aberration. Astronomy Engine models moving bodies; planning precision is about one arcminute, not telescope pointing accuracy. Windows use one-minute bracketing; boundaries are refined to one second and displayed to the minute.").arg(root.controller.catalogCount - 8)
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                        Label {
                            text: (root.controller.result.startUtc || "") + " → " + (root.controller.result.endUtc || "")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                    }
                }
            }
        }
        footer: DialogButtonBox {
            Button {
                text: qsTr("Close")
                onClicked: optionsDialog.close()
            }
        }
    }
    Dialog {
        id: objectChooser
        objectName: "observationObjectChooser"
        property bool favouritesOnly: false
        modal: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        parent: Overlay.overlay
        width: Math.min(600, root.width - 48)
        height: Math.min(600, root.height - 48)
        x: parent ? (parent.width - width) / 2 : 0
        y: parent ? (parent.height - height) / 2 : 0
        padding: 20
        background: Rectangle {
            color: Atelier.surface
            radius: 10
            border.color: Atelier.border
        }
        onOpened: query.focusInput()
        contentItem: ColumnLayout {
            spacing: 12
            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: qsTr("Choose an object")
                    color: Atelier.text
                    font.pixelSize: 18
                    Layout.fillWidth: true
                }
                IconButton {
                    text: qsTr("Close")
                    onClicked: objectChooser.close()
                }
            }
            Label {
                text: qsTr("Selected: %1").arg(root.controller.result.target || "")
                color: Atelier.accent
                wrapMode: Text.Wrap
                Layout.fillWidth: true
                font.pixelSize: 13
            }
            ObservationField {
                id: query
                objectName: "observationSearch"
                Layout.fillWidth: true
                label: qsTr("Name or catalogue number")
                help: qsTr("Examples: Moon, Jupiter, M31, M42, NGC 7000. Typing filters the list. Select a result to use it.")
                onTextChanged: root.controller.search(text)
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                IconButton {
                    Layout.fillWidth: true
                    text: (!objectChooser.favouritesOnly ? "✓ " : "") + qsTr("Catalogue")
                    checkable: true
                    checked: !objectChooser.favouritesOnly
                    borderColor: checked ? Atelier.accent : Atelier.border
                    onClicked: objectChooser.favouritesOnly = false
                }
                IconButton {
                    Layout.fillWidth: true
                    text: (objectChooser.favouritesOnly ? "✓ " : "") + qsTr("Favourites · %1").arg(root.controller.favoriteTargets.length)
                    checkable: true
                    checked: objectChooser.favouritesOnly
                    borderColor: checked ? Atelier.accent : Atelier.border
                    onClicked: objectChooser.favouritesOnly = true
                }
            }
            ListView {
                id: objectList
                objectName: "observationTarget"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 4
                model: objectChooser.favouritesOnly ? root.controller.favoriteTargets.filter(value => value.name.toLocaleLowerCase().indexOf(query.text.toLocaleLowerCase()) >= 0) : root.controller.searchResults
                ScrollBar.vertical: ScrollBar {}
                delegate: IconButton {
                    required property var modelData
                    width: objectList.width
                    text: (root.controller.settings.target === modelData.id ? "✓ " : "") + modelData.name
                    checkable: true
                    checked: root.controller.settings.target === modelData.id
                    borderColor: checked ? Atelier.accent : Atelier.border
                    Accessible.name: qsTr("Select %1").arg(modelData.name)
                    onClicked: root.chooseTarget(modelData.id)
                }
                Label {
                    anchors.centerIn: parent
                    visible: objectList.count === 0
                    text: objectChooser.favouritesOnly ? qsTr("No favourite matches this search.") : qsTr("No local result. Try another name or search online.")
                    width: parent.width
                    color: Atelier.secondary
                    wrapMode: Text.Wrap
                    font.pixelSize: 13
                }
            }
            Label {
                visible: root.controller.searchStatus.length > 0
                text: root.controller.searchStatus
                color: Atelier.secondary
                Layout.fillWidth: true
                wrapMode: Text.Wrap
                font.pixelSize: 12
            }
            IconButton {
                visible: !objectChooser.favouritesOnly && query.text.trim().length > 0
                text: root.controller.searching ? qsTr("Searching…") : qsTr("Search other names online")
                Layout.fillWidth: true
                busy: root.controller.searching
                enabled: !root.controller.searching
                onClicked: root.controller.searchOnline(query.text)
            }
        }
    }
}
