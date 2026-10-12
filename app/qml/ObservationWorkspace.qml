import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier

Rectangle {
    id: root
    objectName: "observationWorkspace"
    color: Atelier.canvas
    required property var controller
    property string viewMode: "prepare"
    property string suggestionKind: "all"
    property int suggestionLimit: 4
    readonly property var filteredSuggestions: controller.recommendations.filter(value => suggestionKind === "all" || (suggestionKind === "solar" ? value.body >= 0 : value.body < 0))
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
            timeUpdates.stop();
            timeUpdatePending = false;
            referenceNotice = "";
            referenceChoices = [];
            controller.setReferenceUtc(plan.startUtc);
            timeSlider.value = controller.instant.fraction;
        }
    }
    property var nightState: ({})
    readonly property bool hasPlot: ["night", "sky", "world", "clouds"].indexOf(viewMode) >= 0
    function cloudPercent(value) {
        return value === undefined || value === null ? "—" : Math.round(Number(value)) + " %";
    }
    function favoriteSiteIndex() {
        const current = controller.settings;
        for (let i = 0; i < controller.sites.length; i++) {
            const site = controller.sites[i];
            if (site.favorite && Number(site.latitude) === Number(current.latitude) && Number(site.longitude) === Number(current.longitude) && Number(site.elevation) === Number(current.elevation) && site.zone === current.zone)
                return i;
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
        if (moved && options.date === current.date) {
            dateNotice = "";
            return controller.changeSite(options);
        }
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
        return controller.calculate(options);
    }
    function sync() {
        if (controller.error.length)
            return;
        syncing = true;
        const s = controller.settings;
        latitude.text = String(s.latitude);
        longitude.text = String(s.longitude);
        elevation.text = String(s.elevation);
        timezone.text = s.zone;
        dateInput.text = s.date;
        minimum.value = s.minimum;
        sitePicker.currentIndex = -1;
        for (let i = 0; i < controller.sites.length; i++) {
            const site = controller.sites[i];
            if (site.name === s.name && Number(site.latitude) === Number(s.latitude) && Number(site.longitude) === Number(s.longitude)) {
                sitePicker.currentIndex = i;
                break;
            }
        }
        darkness.currentIndex = [-18, -12, -6, 90].indexOf(Number(s.sunLimit));
        rightAscension.text = String(s.ra || 0);
        declination.text = String(s.dec || 0);
        timeSlider.value = controller.instant.fraction || 0;
        syncing = false;
        checkNight();
        syncReference();
    }
    function calculate() {
        if (syncing)
            return;
        let s = Object.assign({}, controller.settings, {
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
        applyOptions(s);
    }
    function shiftedDate(days) {
        let date = new Date(controller.settings.date + "T12:00:00Z");
        if (isNaN(date.getTime()))
            return "";
        date.setUTCDate(date.getUTCDate() + days);
        return date.toISOString().slice(0, 10);
    }
    function stepDay(days) {
        dateInput.text = shiftedDate(days);
        calculate();
    }
    function currentNight() {
        history.checked = false;
        dateInput.text = controller.planningNight(controller.settings).currentDate;
        calculate();
    }
    function chooseTarget(id) {
        manual = false;
        if (controller.selectTarget(id)) {
            objectChooser.close();
            viewMode = "night";
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
        function onReferenceChanged() {
            root.syncReference();
        }
    }
    Component.onCompleted: {
        sync();
        ready = true;
    }
    onVisibleChanged: if (ready && visible)
        checkNight()
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
                text: qsTr("Choose a place and a night, then find a visible object or study your selected target. Each view answers one question. Detailed calculations are in the expandable sections.")
            }
        }
        Label {
            Layout.fillWidth: true
            Layout.leftMargin: 20
            Layout.rightMargin: 20
            Layout.bottomMargin: 12
            visible: text.length > 0
            text: root.controller.connectionStatus.length > 0 ? root.controller.connectionStatus : root.controller.weather.failed && root.viewMode !== "clouds" ? root.controller.weather.status : ""
            color: Atelier.warning
            wrapMode: Text.Wrap
            font.pixelSize: 12
            Accessible.role: Accessible.AlertMessage
        }
        Rectangle {
            Layout.fillWidth: true
            height: 1
            color: Atelier.border
        }
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 0
            ScrollView {
                id: mainScroll
                objectName: "observationScroll"
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                ColumnLayout {
                    width: mainScroll.availableWidth
                    spacing: 16
                    ColumnLayout {
                        objectName: "observationWelcome"
                        visible: root.viewMode === "prepare"
                        Layout.fillWidth: true
                        Layout.margins: 32
                        Layout.topMargin: root.height < 720 ? 32 : 72
                        spacing: 20
                        Label {
                            text: qsTr("Your next night under the stars")
                            color: Atelier.text
                            font.pixelSize: root.width < 1000 ? 24 : 30
                            font.weight: Font.Medium
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Label {
                            text: qsTr("Set your place and night on the right. Then choose where to start.")
                            color: Atelier.secondary
                            font.pixelSize: 14
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            implicitHeight: startChoices.implicitHeight + 40
                            color: Atelier.surface
                            radius: Atelier.controlRadius
                            border.color: Atelier.border
                            ColumnLayout {
                                id: startChoices
                                anchors.left: parent.left
                                anchors.right: parent.right
                                anchors.top: parent.top
                                anchors.margins: 20
                                spacing: 12
                                Label {
                                    text: qsTr("Looking for ideas?")
                                    color: Atelier.text
                                    font.pixelSize: 16
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                Label {
                                    text: qsTr("Find objects visible from your place on this night.")
                                    color: Atelier.secondary
                                    font.pixelSize: 13
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                IconButton {
                                    objectName: "observationStartSuggestions"
                                    text: qsTr("Find a visible object")
                                    primary: true
                                    Layout.fillWidth: true
                                    enabled: !root.nightState.past || history.checked
                                    onClicked: root.viewMode = "suggestions"
                                }
                                Rectangle {
                                    Layout.fillWidth: true
                                    height: 1
                                    color: Atelier.border
                                    Layout.topMargin: 8
                                    Layout.bottomMargin: 8
                                }
                                Label {
                                    text: qsTr("Already have a target?")
                                    color: Atelier.text
                                    font.pixelSize: 16
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                }
                                IconButton {
                                    text: qsTr("Study %1").arg(root.controller.result.target || "")
                                    Layout.fillWidth: true
                                    enabled: !root.nightState.past || history.checked
                                    onClicked: root.viewMode = "night"
                                }
                            }
                        }
                    }
                    RowLayout {
                        visible: root.viewMode !== "prepare"
                        Layout.fillWidth: true
                        Layout.topMargin: 20
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 8
                        Repeater {
                            model: [
                                {
                                    key: "suggestions",
                                    label: qsTr("Suggestions")
                                },
                                {
                                    key: "night",
                                    label: qsTr("Night")
                                },
                                {
                                    key: "sky",
                                    label: qsTr("Sky")
                                },
                                {
                                    key: "world",
                                    label: qsTr("World")
                                },
                                {
                                    key: "clouds",
                                    label: qsTr("Clouds")
                                }
                            ]
                            IconButton {
                                required property var modelData
                                Layout.fillWidth: true
                                text: (root.viewMode === modelData.key ? "✓ " : "") + modelData.label
                                checkable: true
                                checked: root.viewMode === modelData.key
                                borderColor: root.viewMode === modelData.key ? Atelier.accent : Atelier.border
                                onClicked: {
                                    root.viewMode = modelData.key;
                                    if (modelData.key === "clouds")
                                        root.controller.refreshWeather(false);
                                }
                                Accessible.description: root.viewMode === modelData.key ? qsTr("Selected view") : ""
                            }
                        }
                    }
                    ColumnLayout {
                        visible: root.viewMode !== "prepare"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 6
                        RowLayout {
                            Layout.fillWidth: true
                            Label {
                                objectName: "observationViewTitle"
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                font.pixelSize: 18
                                font.weight: Font.Medium
                                color: Atelier.text
                                text: root.viewMode === "suggestions" ? qsTr("What can I observe?") : root.viewMode === "world" ? qsTr("Where is %1 visible?").arg((root.controller.result.target || "").split(" · ")[0]) : root.viewMode === "sky" ? qsTr("Where to look for %1?").arg((root.controller.result.target || "").split(" · ")[0]) : root.viewMode === "clouds" ? qsTr("Clouds above %1").arg(root.controller.settings.name || "") : root.controller.result.target || ""
                            }
                            HelpIcon {
                                text: root.viewMode === "suggestions" ? qsTr("Calculated offline from %1 common targets and your favourites. Up to 4 Solar System targets and 6 deep-sky objects, with favourites first, then ranked by available time and height. Each has at least 20 minutes with the displayed limits. Visibility does not guarantee easy viewing: equipment, light pollution and the Moon also matter.").arg(root.controller.result.recommendationPool || 0) : root.viewMode === "world" ? qsTr("This map shows the selected object's geometric visibility at one UTC instant. Height and sky brightness are checked at each grid point. It is not a day/night map or a cloud map. Resolution: 6°. Clicking chooses a place and keeps the instant, with UTC as its initial time zone.") : root.viewMode === "clouds" ? qsTr("0% means no clouds predicted in the model grid cell; 100% means complete coverage. Hourly values are forecasts, not live measurements. Cloud cover alone does not predict transparency or atmospheric turbulence.") : root.viewMode === "sky" ? qsTr("A compass view of the sky around your place: north at the top, east on the right. The edge is the theoretical horizon (0°); the centre is overhead, the zenith (90°). The point shows the selected object at the cursor time; the line is its path over the night. Use the cardinal direction and height to know where to look. This is a diagram, not a globe, photograph or the actual terrain horizon.") : qsTr("Blue intervals meet the selected height and darkness limits. Drag the time slider or click the curve. Terrain and weather are not included in geometric visibility.")
                            }
                        }
                        Label {
                            text: (root.controller.settings.name || "") + " · " + (root.controller.settings.date || "")
                            color: Atelier.secondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            visible: root.viewMode !== "world"
                        }
                        Label {
                            visible: root.viewMode === "world"
                            text: qsTr("Visibility of the selected object · %1").arg(root.controller.instant.utc || "")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 13
                        }
                        Label {
                            visible: root.controller.reference.differentZones && (root.viewMode === "night" || root.viewMode === "clouds")
                            text: qsTr("Chart hours: %1 (telescope)").arg(root.controller.reference.siteZone || "")
                            color: Atelier.secondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: root.viewMode === "sky"
                            text: qsTr("Compass view · edge: horizon · centre: overhead")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                        Label {
                            visible: history.checked && root.nightState.past
                            text: qsTr("Past observation · review mode")
                            color: Atelier.warning
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                    }
                    ColumnLayout {
                        visible: root.viewMode === "suggestions"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 12
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Repeater {
                                model: [
                                    {
                                        key: "all",
                                        label: qsTr("All")
                                    },
                                    {
                                        key: "solar",
                                        label: qsTr("Moon & planets")
                                    },
                                    {
                                        key: "fixed",
                                        label: qsTr("Deep sky")
                                    }
                                ]
                                IconButton {
                                    required property var modelData
                                    Layout.fillWidth: true
                                    text: (root.suggestionKind === modelData.key ? "✓ " : "") + modelData.label
                                    checkable: true
                                    checked: root.suggestionKind === modelData.key
                                    borderColor: root.suggestionKind === modelData.key ? Atelier.accent : Atelier.border
                                    onClicked: {
                                        root.suggestionKind = modelData.key;
                                        root.suggestionLimit = 4;
                                    }
                                    Accessible.description: root.suggestionKind === modelData.key ? qsTr("Selected category") : ""
                                }
                            }
                        }
                        Label {
                            visible: root.filteredSuggestions.length === 0
                            text: qsTr("No suggestion meets these limits here on this date. Try lowering the minimum height or choosing another night. You can still search the complete catalogue.")
                            color: Atelier.warning
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                        }
                        Repeater {
                            model: root.filteredSuggestions.slice(0, root.suggestionLimit)
                            Rectangle {
                                required property var modelData
                                readonly property var clouds: root.controller.recommendationWeather[modelData.id] || ({})
                                Layout.fillWidth: true
                                implicitHeight: suggestionContent.implicitHeight + 24
                                color: Atelier.surface
                                radius: Atelier.controlRadius
                                border.color: Atelier.border
                                ColumnLayout {
                                    id: suggestionContent
                                    anchors.left: parent.left
                                    anchors.right: parent.right
                                    anchors.top: parent.top
                                    anchors.margins: 12
                                    spacing: 6
                                    RowLayout {
                                        Layout.fillWidth: true
                                        spacing: 12
                                        ColumnLayout {
                                            Layout.fillWidth: true
                                            spacing: 4
                                            Label {
                                                text: modelData.name
                                                color: Atelier.text
                                                font.pixelSize: 14
                                                font.weight: Font.Medium
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                            }
                                            Label {
                                                text: modelData.category + qsTr(" · %1 min available").arg(modelData.minutes)
                                                color: Atelier.accent
                                                font.pixelSize: 12
                                                wrapMode: Text.Wrap
                                                Layout.fillWidth: true
                                            }
                                        }
                                        IconButton {
                                            objectName: "observationSuggestion:" + modelData.id
                                            text: qsTr("Explore")
                                            helpText: qsTr("Minimum height: %1°").arg(modelData.minimum) + "\n" + (modelData.sunLimit === -18 ? qsTr("Very dark sky · Sun below −18°") : qsTr("After civil twilight · Sun below −6°")) + "\n" + qsTr("Highest within these windows: %1° · %2").arg(Number(modelData.peakAltitude).toFixed(1)).arg(modelData.peakTime)
                                            Accessible.name: qsTr("Explore %1").arg(modelData.name)
                                            onClicked: root.chooseTarget(modelData.id)
                                        }
                                    }
                                    Label {
                                        text: modelData.slots.map(slot => slot.start + " → " + slot.end).join(" · ")
                                        color: Atelier.text
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                    Label {
                                        visible: clouds.available === true
                                        text: qsTr("Forecast clouds: %1–%2%").arg(Math.round(clouds.minimum || 0)).arg(Math.round(clouds.maximum || 0)) + (root.controller.weather.stale ? qsTr(" · old forecast") : "")
                                        color: root.controller.weather.stale ? Atelier.warning : Atelier.secondary
                                        font.pixelSize: 12
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                    }
                                }
                            }
                        }
                        IconButton {
                            visible: root.filteredSuggestions.length > root.suggestionLimit
                            text: qsTr("Show %1 more suggestions").arg(root.filteredSuggestions.length - root.suggestionLimit)
                            Layout.fillWidth: true
                            onClicked: root.suggestionLimit = 100
                        }
                    }
                    RowLayout {
                        visible: root.viewMode === "night" || root.viewMode === "sky"
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        Layout.fillWidth: true
                        spacing: 16
                        Label {
                            text: root.viewMode === "sky" ? (root.controller.instant.altitude < 0 ? qsTr("Below horizon") : qsTr("Look %1 · height %2°").arg(root.compassDirection(root.controller.instant.azimuth || 0)).arg(Number(root.controller.instant.altitude || 0).toFixed(1))) : qsTr("Height: %1°").arg(Number(root.controller.instant.altitude || 0).toFixed(1))
                            color: Atelier.text
                            font.pixelSize: 16
                        }
                        Label {
                            text: root.controller.instant.status || ""
                            color: Atelier.accent
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                    }
                    ColumnLayout {
                        visible: root.viewMode === "clouds"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
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
                        id: plot
                        visible: root.hasPlot
                        objectName: "observationPlot"
                        Layout.fillWidth: true
                        Layout.preferredHeight: root.viewMode === "world" ? Math.min((mainScroll.availableWidth - 32) / 2 + 40, Math.max(200, root.height - (root.controller.connectionStatus.length > 0 ? 370 : 340) - (root.controller.reference.differentZones ? 24 : 0))) : root.viewMode === "clouds" && root.width < 1100 ? 170 : root.viewMode === "night" && root.width < 1100 ? (root.controller.reference.differentZones ? 170 : 220) : 250
                        Layout.leftMargin: 12
                        Layout.rightMargin: 12
                        controller: root.controller
                        mode: root.viewMode
                        colors: ({
                                text: Atelier.text,
                                secondary: Atelier.secondary,
                                border: Atelier.border,
                                accent: Atelier.accent,
                                surface: Atelier.surface,
                                image: Atelier.image,
                                primary: Atelier.primary,
                                warning: Atelier.warning
                            })
                        Accessible.role: Accessible.Graphic
                        Accessible.name: root.viewMode === "world" ? qsTr("World visibility map at the selected UTC instant") : root.viewMode === "sky" ? qsTr("Object's trajectory above the horizon") : root.viewMode === "clouds" ? qsTr("Hourly forecast cloud coverage from 0 to 100 percent over the selected period") : qsTr("Height curve over the chosen night, with eligible intervals in blue")
                        onFractionChosen: fraction => {
                            timeSlider.value = fraction;
                            root.scheduleTimeUpdate();
                        }
                        onLocationChosen: (latitude, longitude) => {
                            root.manual = false;
                            root.controller.chooseMapLocation(latitude, longitude);
                        }
                    }
                    Flow {
                        visible: root.viewMode === "world"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
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
                    ColumnLayout {
                        visible: root.hasPlot
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 8
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                objectName: "observationReferenceReadout"
                                text: (root.controller.reference.source === "site" ? qsTr("At the telescope: %1") : qsTr("At home: %1")).arg(root.controller.reference.display || "")
                                color: Atelier.text
                                font.pixelSize: 13
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                text: root.controller.instant.solarPhase || ""
                                color: Atelier.accent
                                font.pixelSize: 12
                                visible: root.viewMode === "night" || root.viewMode === "sky"
                            }
                            HelpIcon {
                                visible: root.viewMode === "world"
                                text: qsTr("The whole map uses the same UTC instant. Moving the slider changes when the selected object can meet your height and darkness limits around the world.")
                            }
                        }
                        Label {
                            visible: root.controller.reference.differentZones === true
                            text: (root.controller.reference.source === "site" ? qsTr("At home: %1") : qsTr("At the telescope: %1")).arg(root.controller.reference.source === "site" ? root.controller.reference.homeDisplay || "" : root.controller.reference.siteDisplay || "")
                            color: Atelier.secondary
                            font.pixelSize: 12
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
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
                    }
                    ColumnLayout {
                        objectName: "observationWindows"
                        visible: root.viewMode === "night" || root.viewMode === "sky"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        spacing: 8
                        SettingLabel {
                            visible: root.viewMode === "night" && !(history.checked && root.nightState.past)
                            text: qsTr("When to start recording?")
                            explanation: qsTr("The first remaining window is chosen after the current real time, independently of the exploratory cursor. It meets the selected height and darkness limits. Start is rounded up to a full minute; end is rounded down. Reference time converts the same instants. Check weather, terrain and telescope setup separately.")
                        }
                        ColumnLayout {
                            visible: root.viewMode === "night" && !(history.checked && root.nightState.past) && root.controller.reference.recording.available === true
                            Layout.fillWidth: true
                            spacing: 6
                            RowLayout {
                                Layout.fillWidth: true
                                spacing: 12
                                Label {
                                    objectName: "observationRecordingStart"
                                    text: (root.controller.reference.source === "site" ? qsTr("Telescope: from %1") : qsTr("At home: from %1")).arg(root.controller.reference.recording.start || "")
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
                                text: qsTr("Until %1 · %2 min").arg(root.controller.reference.recording.end || "").arg(root.controller.reference.recording.minutes || 0)
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                visible: root.controller.reference.differentZones
                                text: (root.controller.reference.source === "site" ? qsTr("At home: %1 → %2") : qsTr("At the telescope: %1 → %2")).arg(root.controller.reference.source === "site" ? root.controller.reference.recording.homeStart || "" : root.controller.reference.recording.siteStart || "").arg(root.controller.reference.source === "site" ? root.controller.reference.recording.homeEnd || "" : root.controller.reference.recording.siteEnd || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                        Label {
                            visible: root.viewMode === "night" && !(history.checked && root.nightState.past) && !root.controller.reference.recording.available
                            text: qsTr("No remaining recording window in this night with the selected limits.")
                            color: Atelier.secondary
                            wrapMode: Text.Wrap
                            Layout.fillWidth: true
                            font.pixelSize: 12
                        }
                        InspectorFamily {
                            objectName: "observationAllWindows"
                            visible: root.viewMode === "night" || history.checked
                            title: qsTr("All windows · %1").arg(root.controller.reference.zone || "")
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
                                        text: modelData.referenceStart + " → " + modelData.referenceEnd
                                        color: Atelier.text
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                        font.pixelSize: 13
                                    }
                                    Label {
                                        visible: root.controller.reference.differentZones
                                        text: (root.controller.reference.source === "site" ? qsTr("At home: %1 → %2") : qsTr("At the telescope: %1 → %2")).arg(root.controller.reference.source === "site" ? modelData.homeStart : modelData.siteStart).arg(root.controller.reference.source === "site" ? modelData.homeEnd : modelData.siteEnd)
                                        color: Atelier.secondary
                                        wrapMode: Text.Wrap
                                        Layout.fillWidth: true
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 12
                            Label {
                                text: (root.controller.reference.differentZones ? qsTr("Sunset at telescope: %1") : qsTr("Sunset: %1")).arg(root.controller.result.sunset || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: (root.controller.reference.differentZones ? qsTr("Sunrise at telescope: %1") : qsTr("Sunrise: %1")).arg(root.controller.result.sunrise || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                        }
                        InspectorFamily {
                            objectName: "observationDetails"
                            title: qsTr("Details · Moon, direction & solar times")
                            onActivated: expanded = !expanded
                            Label {
                                text: qsTr("Direction: %1° from north").arg(Number(root.controller.instant.azimuth || 0).toFixed(1))
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("Highest in this period: %1 · %2°").arg(root.controller.result.peakTime || "").arg(Number(root.controller.result.peakAltitude || 0).toFixed(1))
                                color: Atelier.secondary
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                text: root.controller.result.period || ""
                                color: Atelier.secondary
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            Repeater {
                                model: root.controller.result.events || []
                                Label {
                                    required property var modelData
                                    text: modelData.name + " · " + modelData.time
                                    color: Atelier.secondary
                                    Layout.fillWidth: true
                                    wrapMode: Text.Wrap
                                }
                            }
                            Label {
                                visible: root.controller.result.noAstronomicalNight || false
                                text: qsTr("There is no astronomical night during this period (Sun never below −18°).")
                                color: Atelier.warning
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                            }
                            Label {
                                text: root.controller.result.moonTarget ? qsTr("Moon: %1% illuminated · height %2°").arg(Number(root.controller.instant.moonFraction || 0).toFixed(0)).arg(Number(root.controller.instant.moonAltitude || 0).toFixed(1)) : qsTr("Moon: %1% illuminated · height %2° · %3° from the target").arg(Number(root.controller.instant.moonFraction || 0).toFixed(0)).arg(Number(root.controller.instant.moonAltitude || 0).toFixed(1)).arg(Number(root.controller.instant.moonSeparation || 0).toFixed(1))
                                color: Atelier.secondary
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            SettingLabel {
                                text: qsTr("Day, twilight and night")
                                explanation: qsTr("Sunrise and sunset use the Sun's upper edge with standard refraction at your elevation, over a flat horizon. Twilight and astronomical night use the geometric centre of the Sun at −6° and −18°. Mountains, buildings and actual weather can change the observed times.")
                            }
                            Repeater {
                                model: root.controller.result.daylightSlots || []
                                Label {
                                    required property var modelData
                                    text: qsTr("Daylight: %1 → %2").arg(modelData.start).arg(modelData.end)
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                }
                            }
                            Repeater {
                                model: root.controller.result.nightSlots || []
                                Label {
                                    required property var modelData
                                    text: qsTr("Astronomical night: %1 → %2").arg(modelData.start).arg(modelData.end)
                                    color: Atelier.accent
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                }
                            }
                            Repeater {
                                model: root.controller.result.solarEvents || []
                                Label {
                                    required property var modelData
                                    text: modelData.name + " · " + modelData.time
                                    color: Atelier.secondary
                                    wrapMode: Text.Wrap
                                    Layout.fillWidth: true
                                    font.pixelSize: 12
                                }
                            }
                        }
                    }
                    InspectorFamily {
                        visible: root.viewMode === "clouds"
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
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
                        visible: root.viewMode === "clouds" && root.controller.weather.hasForecast
                        Layout.fillWidth: true
                        Layout.leftMargin: 20
                        Layout.rightMargin: 20
                        text: qsTr("Forecast: <a href='https://open-meteo.com/'>Open-Meteo</a> · <a href='https://creativecommons.org/licenses/by/4.0/'>CC BY 4.0</a>")
                        textFormat: Text.StyledText
                        linkColor: Atelier.accent
                        color: Atelier.secondary
                        wrapMode: Text.Wrap
                        font.pixelSize: 12
                        onLinkActivated: link => Qt.openUrlExternally(link)
                    }
                    Item {
                        Layout.preferredHeight: 16
                    }
                }
            }
            Rectangle {
                width: 1
                Layout.fillHeight: true
                color: Atelier.border
            }
            Rectangle {
                Layout.preferredWidth: root.width < 1100 ? 300 : 340
                Layout.fillHeight: true
                color: Atelier.surface
                ScrollView {
                    id: inspector
                    objectName: "observationInspector"
                    anchors.fill: parent
                    anchors.margins: 16
                    contentWidth: availableWidth
                    clip: true
                    ColumnLayout {
                        width: inspector.availableWidth
                        spacing: 14
                        SettingLabel {
                            text: qsTr("Selected object")
                            explanation: qsTr("This is the only active target. Search and favourites are grouped in Change object. Typing filters the list; choosing a row changes the target.")
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            Label {
                                objectName: "observationSelectedTarget"
                                text: root.controller.result.target || ""
                                color: Atelier.text
                                font.pixelSize: 14
                                font.weight: Font.Medium
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            IconButton {
                                objectName: "observationFavoriteTarget"
                                text: root.controller.targetFavorite ? "★" : "☆"
                                Layout.preferredWidth: 36
                                Accessible.name: root.controller.targetFavorite ? qsTr("★ Remove object from favourites") : qsTr("☆ Save object to favourites")
                                onClicked: root.controller.toggleFavoriteTarget()
                                DarkToolTip {
                                    visible: parent.hovered || parent.activeFocus
                                    text: parent.Accessible.name
                                }
                            }
                        }
                        IconButton {
                            objectName: "observationChooseTarget"
                            text: qsTr("Change object…")
                            Layout.fillWidth: true
                            onClicked: root.openObjectChooser()
                        }
                        SettingLabel {
                            text: qsTr("From where?")
                            explanation: qsTr("The first launch uses central Paris, then the last chosen place is restored. There is no automatic geolocation. Choose a city or enter coordinates in Advanced. Name this place saves its label, coordinates and time zone as a favourite; Rename changes an existing favourite.")
                        }
                        DarkComboBox {
                            id: sitePicker
                            objectName: "observationSite"
                            Layout.fillWidth: true
                            model: root.controller.sites
                            textRole: "label"
                            displayText: root.controller.settings.name || qsTr("Custom location")
                            explanation: qsTr("The city's time zone follows local daylight-saving rules. Changing place keeps the UTC instant; local times and the night date are converted. A map click initially uses UTC.")
                            onActivated: index => {
                                root.manual = false;
                                let s = Object.assign({}, root.controller.settings, root.controller.sites[index]);
                                root.applyOptions(s);
                            }
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 8
                            IconButton {
                                Layout.fillWidth: true
                                text: root.favoriteSiteIndex() >= 0 ? qsTr("Rename…") : qsTr("Name this place…")
                                objectName: "observationNamePlace"
                                Accessible.name: root.favoriteSiteIndex() >= 0 ? qsTr("Rename this saved place") : qsTr("Name and save this place")
                                onClicked: {
                                    root.saveSiteOpen = !root.saveSiteOpen;
                                    favoriteName.text = root.controller.settings.name || "";
                                    if (root.saveSiteOpen)
                                        favoriteName.focusInput(true);
                                }
                            }
                            IconButton {
                                Layout.fillWidth: true
                                text: qsTr("Another city…")
                                onClicked: root.citySearchOpen = !root.citySearchOpen
                            }
                        }
                        ObservationField {
                            id: favoriteName
                            objectName: "observationPlaceName"
                            visible: root.saveSiteOpen
                            Layout.fillWidth: true
                            label: qsTr("Place name")
                            help: qsTr("For example Garden, Balcony or Mountain site. The name, coordinates and time zone are saved on this computer as a favourite, including points chosen on the map. Renaming keeps the same position.")
                        }
                        RowLayout {
                            visible: root.saveSiteOpen
                            Layout.fillWidth: true
                            spacing: 8
                            IconButton {
                                Layout.fillWidth: true
                                text: qsTr("Save this site")
                                enabled: favoriteName.text.trim().length > 0
                                onClicked: {
                                    if (root.controller.saveSite(favoriteName.text))
                                        root.saveSiteOpen = false;
                                }
                            }
                            IconButton {
                                Layout.fillWidth: true
                                text: qsTr("Remove favourite")
                                visible: root.favoriteSiteIndex() >= 0
                                onClicked: {
                                    root.controller.removeSite(root.favoriteSiteIndex());
                                    root.saveSiteOpen = false;
                                }
                            }
                        }
                        ObservationField {
                            id: cityQuery
                            visible: root.citySearchOpen
                            Layout.fillWidth: true
                            label: qsTr("Another city")
                            help: qsTr("Enter a city name, optionally followed by its country. Searching sends only this text to Open-Meteo / GeoNames. No weather is requested.")
                        }
                        IconButton {
                            visible: root.citySearchOpen
                            Layout.fillWidth: true
                            text: root.controller.locating ? qsTr("Searching…") : qsTr("Find this city online")
                            busy: root.controller.locating
                            enabled: !root.controller.locating && cityQuery.text.trim().length >= 2
                            onClicked: root.controller.searchCity(cityQuery.text)
                        }
                        Label {
                            visible: root.citySearchOpen && root.controller.locationStatus.length > 0
                            text: root.controller.locationStatus
                            color: Atelier.secondary
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
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
                                    if (root.applyOptions(Object.assign({}, root.controller.settings, modelData)))
                                        root.citySearchOpen = false;
                                }
                            }
                        }
                        SettingLabel {
                            text: qsTr("Which night?")
                            explanation: qsTr("The date is local to the chosen place, from noon to the following noon. Before sunrise, the ongoing night can still have yesterday's date. Passed nights require review mode in Advanced.")
                        }
                        ObservationField {
                            id: dateInput
                            objectName: "observationDate"
                            Layout.fillWidth: true
                            label: qsTr("Local date · YYYY-MM-DD")
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
                                enabled: history.checked || (!root.controller.planningNight(Object.assign({}, root.controller.settings, {
                                        date: root.shiftedDate(-1)
                                    })).past)
                                onClicked: root.stepDay(-1)
                            }
                            IconButton {
                                objectName: "observationCurrentNight"
                                text: qsTr("Current night")
                                Layout.fillWidth: true
                                onClicked: root.currentNight()
                            }
                            IconButton {
                                text: "›"
                                Layout.preferredWidth: 32
                                Accessible.name: qsTr("Next night")
                                onClicked: root.stepDay(1)
                            }
                        }
                        Label {
                            objectName: "observationDateNotice"
                            visible: root.dateNotice.length > 0 || (root.nightState.past && !history.checked)
                            text: root.dateNotice.length > 0 ? root.dateNotice : qsTr("This night has ended. Choose Current night to continue planning.")
                            color: Atelier.warning
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            font.pixelSize: 12
                            Accessible.role: Accessible.AlertMessage
                        }
                        InspectorFamily {
                            id: referenceSection
                            objectName: "observationReferenceSection"
                            title: qsTr("Reference time · %1").arg(root.controller.reference.clock || "—")
                            onActivated: expanded = !expanded
                            SettingLabel {
                                text: qsTr("Time zone for the reference hour")
                                explanation: qsTr("At home follows this Mac's time zone. At the telescope follows the observing place. Changing this choice converts times without moving the site, changing the night or changing the UTC instant.")
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
                            DarkComboBox {
                                id: referenceDay
                                objectName: "observationReferenceDay"
                                Layout.fillWidth: true
                                model: root.controller.reference.days || []
                                textRole: "label"
                                Accessible.name: qsTr("Date in the reference time zone")
                                explanation: qsTr("Choose the calendar day in the reference time zone. A night can cross midnight and a time-zone conversion can change the date.")
                                onActivated: root.applyReferenceTime()
                            }
                            ObservationField {
                                id: referenceClock
                                objectName: "observationReferenceClock"
                                Layout.fillWidth: true
                                label: qsTr("Reference hour · HH:mm")
                                help: qsTr("Enter the time in the selected reference time zone. The cursor and all four views move to the same instant. For example, 23:30. Clock-change gaps are rejected; repeated hours let you choose the offset.")
                                onAccepted: root.applyReferenceTime()
                                onTextEdited: root.referenceClockEdited = true
                            }
                            Label {
                                visible: root.referenceNotice.length > 0
                                text: root.referenceNotice
                                Layout.fillWidth: true
                                wrapMode: Text.Wrap
                                color: Atelier.warning
                                font.pixelSize: 12
                                Accessible.role: Accessible.AlertMessage
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
                                    root.controller.setReferenceUtc(root.referenceChoices[index].utc);
                                    root.referenceChoices = [];
                                    root.referenceNotice = "";
                                    root.referenceClockEdited = false;
                                    referenceClock.text = root.controller.reference.clock;
                                }
                            }
                            Label {
                                text: qsTr("At home: %1").arg(root.controller.reference.homeDisplay || "")
                                color: Atelier.text
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            Label {
                                text: qsTr("At the telescope: %1").arg(root.controller.reference.siteDisplay || "")
                                color: Atelier.secondary
                                font.pixelSize: 12
                                wrapMode: Text.Wrap
                                Layout.fillWidth: true
                            }
                            IconButton {
                                text: qsTr("Show first remaining window")
                                Layout.fillWidth: true
                                enabled: root.controller.reference.recording.available === true
                                onClicked: {
                                    root.showRecordingWindow();
                                    root.viewMode = "night";
                                }
                            }
                        }
                        InspectorFamily {
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
                        Label {
                            visible: root.controller.error.length > 0
                            text: root.controller.error + "\n" + qsTr("The displayed results keep the previous valid choices.")
                            color: Atelier.danger
                            Layout.fillWidth: true
                            wrapMode: Text.Wrap
                            Accessible.role: Accessible.AlertMessage
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
                                    id: latitude
                                    objectName: "observationLatitude"
                                    Layout.fillWidth: true
                                    label: qsTr("Latitude · degrees")
                                    help: qsTr("North is positive; south is negative. Range −90 to +90.")
                                    onAccepted: root.calculate()
                                }
                                ObservationField {
                                    id: longitude
                                    Layout.fillWidth: true
                                    label: qsTr("Longitude · degrees")
                                    help: qsTr("East is positive; west is negative. Range −180 to +180.")
                                    onAccepted: root.calculate()
                                }
                                ObservationField {
                                    id: elevation
                                    objectName: "observationElevation"
                                    Layout.fillWidth: true
                                    label: qsTr("Elevation · metres")
                                    help: qsTr("Height above sea level, −500 to 10000 metres. Sunrise uses standard refraction adjusted for this elevation. Terrain and horizon dip are not modelled.")
                                    onAccepted: root.calculate()
                                }
                                ObservationField {
                                    id: timezone
                                    objectName: "observationZone"
                                    Layout.fillWidth: true
                                    label: qsTr("Time zone")
                                    help: qsTr("Use an IANA name such as Europe/Paris, America/Santiago or UTC. Clock changes are applied automatically.")
                                    onAccepted: root.calculate()
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
