import QtQuick
import QtQuick.Controls.Basic
import QtQuick.Layouts
import "Atelier.js" as Atelier
Rectangle {
    id: root
    objectName: "observationWorkspace"
    color: Atelier.canvas
    required property var controller
    property string viewMode: "suggestions"
    property string suggestionKind: "all"
    readonly property var filteredSuggestions: controller.recommendations.filter(value=>suggestionKind==="all" || (suggestionKind==="solar" ? value.body>=0 : value.body<0))
    property bool syncing: false
    property bool manual: false
    property bool citySearchOpen: false
    property bool saveSiteOpen: false
    function favoriteSiteIndex() {
        const current=controller.settings;
        for(let i=0;i<controller.sites.length;i++) {
            const site=controller.sites[i];
            if(site.favorite && Number(site.latitude)===Number(current.latitude) && Number(site.longitude)===Number(current.longitude) && Number(site.elevation)===Number(current.elevation) && site.zone===current.zone)return i;
        }
        return -1;
    }
    function sync() {
        if (controller.error.length) return;
        syncing=true;
        const s=controller.settings;
        latitude.text=String(s.latitude); longitude.text=String(s.longitude); elevation.text=String(s.elevation);
        timezone.text=s.zone; dateInput.text=s.date; minimum.value=s.minimum;
        sitePicker.currentIndex=-1;
        for(let i=0;i<controller.sites.length;i++) {
            const site=controller.sites[i];
            if(site.name===s.name && Number(site.latitude)===Number(s.latitude) && Number(site.longitude)===Number(s.longitude)){sitePicker.currentIndex=i;break;}
        }
        darkness.currentIndex=[-18,-12,-6,90].indexOf(Number(s.sunLimit));
        rightAscension.text=String(s.ra || 0); declination.text=String(s.dec || 0);
        timeSlider.value=controller.instant.fraction || 0;
        syncing=false;
    }
    function calculate() {
        if(syncing)return;
        let s=Object.assign({},controller.settings,{latitude:latitude.text,longitude:longitude.text,elevation:elevation.text,zone:timezone.text,date:dateInput.text,minimum:minimum.value,sunLimit:[-18,-12,-6,90][darkness.currentIndex],manual:manual,ra:rightAscension.text,dec:declination.text});
        if(Number(s.latitude)!==Number(controller.settings.latitude) || Number(s.longitude)!==Number(controller.settings.longitude))s.name=qsTr("Custom location");
        controller.calculate(s);
    }
    function stepDay(days) {let date=new Date(dateInput.text+"T12:00:00Z");if(isNaN(date.getTime()))return;date.setUTCDate(date.getUTCDate()+days);dateInput.text=date.toISOString().slice(0,10);calculate();}
    Connections {target:root.controller;function onChanged(){root.sync();}}
    Component.onCompleted: sync()
    Timer {id:timeDebounce;interval:120;onTriggered:root.controller.setFraction(timeSlider.value)}
    ColumnLayout {
        anchors.fill:parent;spacing:0
        RowLayout {
            Layout.fillWidth:true;Layout.margins:20;spacing:16
            ColumnLayout {
                Layout.fillWidth:true;spacing:6
                Label {text:qsTr("Prepare an observation");color:Atelier.text;font.pixelSize:20;font.weight:Font.Medium}
                Label {text:qsTr("Choose an object, a place and a night. Explore when it is high enough in the sky.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:13}
            }
            HelpIcon {text:qsTr("Start with the suggestions or type a name, such as Moon, Jupiter, M42 or NGC 7000. Then choose your location and date. Blue intervals satisfy the height and darkness limits. They do not predict weather or guarantee an observation.")}
        }
        Label {
            Layout.fillWidth:true;Layout.leftMargin:20;Layout.rightMargin:20;Layout.bottomMargin:12
            visible:root.controller.connectionStatus.length>0
            text:root.controller.connectionStatus;color:Atelier.warning;wrapMode:Text.Wrap;font.pixelSize:12
            Accessible.role:Accessible.AlertMessage
        }
        Rectangle {Layout.fillWidth:true;height:1;color:Atelier.border}
        RowLayout {
            Layout.fillWidth:true;Layout.fillHeight:true;spacing:0
            ScrollView {
                id:mainScroll;objectName:"observationScroll"
                Layout.fillWidth:true;Layout.fillHeight:true;clip:true
                contentWidth:availableWidth
                ColumnLayout {
                    width:mainScroll.availableWidth;spacing:16
                    RowLayout {
                        Layout.fillWidth:true;Layout.topMargin:20;Layout.leftMargin:20;Layout.rightMargin:20
                        ColumnLayout {Layout.fillWidth:true;spacing:6
                            Label {text:root.controller.result.target || "";color:Atelier.text;font.pixelSize:16;wrapMode:Text.Wrap;Layout.fillWidth:true}
                            Label {text:root.controller.settings.name || qsTr("Custom location");color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        }
                    }
                    RowLayout {
                        Layout.leftMargin:20;Layout.rightMargin:20;Layout.fillWidth:true;spacing:16
                        ColumnLayout {Layout.fillWidth:true;spacing:4
                            Label {text:qsTr("Sunset");color:Atelier.secondary;font.pixelSize:12}
                            Label {text:root.controller.result.sunset || "";color:Atelier.text;font.pixelSize:13;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        }
                        ColumnLayout {Layout.fillWidth:true;spacing:4
                            Label {text:qsTr("Sunrise");color:Atelier.secondary;font.pixelSize:12}
                            Label {text:root.controller.result.sunrise || "";color:Atelier.text;font.pixelSize:13;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        }
                    }
                    Label {Layout.leftMargin:20;Layout.rightMargin:20;Layout.fillWidth:true;text:root.controller.result.polarSun || "";visible:text.length>0;color:Atelier.warning;wrapMode:Text.Wrap;font.pixelSize:12}
                    RowLayout {
                        visible:root.viewMode!=="suggestions";Layout.leftMargin:20;Layout.rightMargin:20;Layout.fillWidth:true;spacing:24
                        ColumnLayout {spacing:4
                            SettingLabel {text:qsTr("Height above the horizon");explanation:qsTr("0° is the horizon; 90° is directly overhead. A higher object usually crosses less atmosphere. Buildings and mountains are not included.")}
                            Label {text:Number(root.controller.instant.altitude || 0).toFixed(1)+"°";color:Atelier.text;font.pixelSize:24}
                        }
                        ColumnLayout {Layout.fillWidth:true;spacing:4
                            SettingLabel {text:qsTr("Direction");explanation:qsTr("Azimuth is measured clockwise from north: north 0°, east 90°, south 180°, west 270°.")}
                            Label {text:Number(root.controller.instant.azimuth || 0).toFixed(1)+"°";color:Atelier.text;font.pixelSize:24}
                        }
                    }
                    Label {Layout.leftMargin:20;Layout.rightMargin:20;Layout.fillWidth:true;visible:root.viewMode!=="suggestions";text:root.controller.instant.status || "";color:Atelier.accent;wrapMode:Text.Wrap;font.pixelSize:13;Accessible.role:Accessible.StaticText}
                    RowLayout {
                        Layout.leftMargin:20;Layout.rightMargin:20;spacing:8
                        Repeater {model:[{key:"suggestions",label:qsTr("Suggestions")},{key:"night",label:qsTr("Night")},{key:"sky",label:qsTr("Horizon")},{key:"world",label:qsTr("World")}]
                            IconButton {required property var modelData;text:(root.viewMode===modelData.key?"✓ ":"")+modelData.label;checkable:true;checked:root.viewMode===modelData.key;borderColor:root.viewMode===modelData.key?Atelier.accent:Atelier.border;onClicked:root.viewMode=modelData.key;Accessible.description:root.viewMode===modelData.key?qsTr("Selected view"):""}
                        }
                    }
                    ColumnLayout {
                        visible:root.viewMode==="suggestions";Layout.fillWidth:true;Layout.leftMargin:20;Layout.rightMargin:20;spacing:12
                        Label {text:qsTr("What can I observe?");color:Atelier.text;font.pixelSize:18;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {text:qsTr("For %1 · %2 · at least %3° above the horizon").arg(root.controller.settings.name || "").arg(root.controller.settings.date || "").arg(root.controller.settings.minimum || 0);color:Atelier.secondary;font.pixelSize:13;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        RowLayout {Layout.fillWidth:true;spacing:8
                            Repeater {model:[{key:"all",label:qsTr("All")},{key:"solar",label:qsTr("Moon & planets")},{key:"fixed",label:qsTr("Deep sky")}]
                                IconButton {required property var modelData;text:(root.suggestionKind===modelData.key?"✓ ":"")+modelData.label;checkable:true;checked:root.suggestionKind===modelData.key;borderColor:root.suggestionKind===modelData.key?Atelier.accent:Atelier.border;onClicked:root.suggestionKind=modelData.key;Accessible.description:root.suggestionKind===modelData.key?qsTr("Selected category"):""}
                            }
                        }
                        Label {text:qsTr("Calculated offline from %1 common targets and your favourites. Up to 4 Solar System targets and 6 deep-sky objects, with favourites first, then ranked by available time and height. Each has at least 20 minutes with the displayed limits.").arg(root.controller.result.recommendationPool || 0);color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Label {visible:root.filteredSuggestions.length===0;text:qsTr("No suggestion meets these limits here on this date. Try lowering the minimum height or choosing another night. You can still search the complete catalogue.");color:Atelier.warning;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Repeater {
                            model:root.filteredSuggestions
                            Rectangle {
                                required property var modelData
                                Layout.fillWidth:true;implicitHeight:suggestionContent.implicitHeight+24
                                color:Atelier.surface;radius:Atelier.controlRadius;border.color:Atelier.border
                                ColumnLayout {
                                    id:suggestionContent;anchors.left:parent.left;anchors.right:parent.right;anchors.top:parent.top;anchors.margins:12;spacing:8
                                    RowLayout {Layout.fillWidth:true;spacing:12
                                        ColumnLayout {Layout.fillWidth:true;spacing:4
                                            Label {text:modelData.name;color:Atelier.text;font.pixelSize:14;font.weight:Font.Medium;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                            Label {text:modelData.category+qsTr(" · %1 min available").arg(modelData.minutes);color:Atelier.accent;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                        }
                                        IconButton {objectName:"observationSuggestion:"+modelData.id;text:qsTr("Explore");Accessible.name:qsTr("Explore %1").arg(modelData.name);onClicked:{root.manual=false;if(root.controller.selectTarget(modelData.id))root.viewMode="night";}}
                                    }
                                    Repeater {model:modelData.slots
                                        Label {required property var modelData;text:modelData.start+" → "+modelData.end;color:Atelier.text;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    }
                                    Label {text:qsTr("Highest within these windows: %1° · %2").arg(Number(modelData.peakAltitude).toFixed(1)).arg(modelData.peakTime);color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                    Label {text:modelData.sunLimit===-18?qsTr("Very dark sky · Sun below −18°"):qsTr("After civil twilight · Sun below −6°");color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                }
                            }
                        }
                        Label {text:qsTr("Visibility does not guarantee easy viewing: equipment, light pollution and the Moon also matter. Uranus, Neptune and deep-sky objects generally need binoculars or a telescope. No weather forecast is included.");color:Atelier.secondary;font.pixelSize:12;wrapMode:Text.Wrap;Layout.fillWidth:true;Layout.bottomMargin:20}
                    }
                    ObservationPlot {
                        visible:root.viewMode!=="suggestions"
                        id:plot;objectName:"observationPlot"
                        Layout.fillWidth:true;Layout.preferredHeight:root.viewMode==="world"?260:250
                        Layout.leftMargin:12;Layout.rightMargin:12
                        controller:root.controller;mode:root.viewMode
                        colors:({text:Atelier.text,secondary:Atelier.secondary,border:Atelier.border,accent:Atelier.accent,surface:Atelier.surface,image:Atelier.image,primary:Atelier.primary,warning:Atelier.warning})
                        Accessible.role:Accessible.Graphic
                        Accessible.name:root.viewMode==="world"?qsTr("World visibility map at the selected UTC instant"):root.viewMode==="sky"?qsTr("Object's trajectory above the horizon"):qsTr("Height curve over the chosen night, with eligible intervals in blue")
                        onFractionChosen:fraction=>{timeSlider.value=fraction;timeDebounce.restart();}
                        onLocationChosen:(latitude,longitude)=>{root.manual=false;root.controller.chooseMapLocation(latitude,longitude);}
                    }
                    Label {Layout.leftMargin:20;Layout.rightMargin:20;Layout.fillWidth:true;visible:root.viewMode==="world";text:qsTr("Dark: below horizon · amber: too low · grey: sky too bright · blue: selected limits met. Regional grid: 6°. Click to choose a place; its time zone is set to UTC.");color:Atelier.secondary;wrapMode:Text.Wrap;font.pixelSize:12}
                    ColumnLayout {
                        visible:root.viewMode!=="suggestions";Layout.fillWidth:true;Layout.leftMargin:20;Layout.rightMargin:20;spacing:8
                        Label {text:root.controller.instant.local || "";color:Atelier.text;font.pixelSize:13;Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Label {text:root.controller.instant.solarPhase || "";color:Atelier.accent;font.pixelSize:12}
                        Label {text:root.controller.instant.utc || "";color:Atelier.secondary;font.pixelSize:12}
                        AtelierSlider {id:timeSlider;objectName:"observationTime";Layout.fillWidth:true;from:0;to:1;value:.5;stepSize:1/1440;Accessible.name:qsTr("Explore the night");onMoved:timeDebounce.restart()}
                        Label {text:qsTr("Curve times are local. Each date covers noon to the following noon, including clock changes.");color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12}
                    }
                    Rectangle {visible:root.viewMode!=="suggestions";Layout.fillWidth:true;Layout.leftMargin:20;Layout.rightMargin:20;height:1;color:Atelier.border}
                    ColumnLayout {
                        visible:root.viewMode!=="suggestions";Layout.fillWidth:true;Layout.leftMargin:20;Layout.rightMargin:20;spacing:10
                        Label {text:qsTr("Observation windows");color:Atelier.text;font.pixelSize:14;font.weight:Font.Medium}
                        Label {visible:(root.controller.result.slots || []).length===0;text:root.controller.result.noDarkness?qsTr("No period is dark enough for the selected limit. For the Moon or a planet, try a brighter-sky limit."):qsTr("No window with these limits. Try a different date, location or minimum height.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        Repeater {model:root.controller.result.slots || []
                            Label {required property var modelData;text:modelData.start+" → "+modelData.end+qsTr(" · %1 min").arg(modelData.minutes);color:Atelier.text;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:13}
                        }
                        Label {text:qsTr("Highest in this period: %1 · %2°").arg(root.controller.result.peakTime || "").arg(Number(root.controller.result.peakAltitude || 0).toFixed(1));color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Label {text:root.controller.result.period || "";color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Repeater {model:root.controller.result.events || [];Label {required property var modelData;text:modelData.name+" · "+modelData.time;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap}}
                        Label {visible:root.controller.result.noAstronomicalNight || false;text:qsTr("There is no astronomical night during this period (Sun never below −18°).");color:Atelier.warning;Layout.fillWidth:true;wrapMode:Text.Wrap}
                        Label {text:root.controller.result.moonTarget?qsTr("Moon: %1% illuminated · height %2°").arg(Number(root.controller.instant.moonFraction || 0).toFixed(0)).arg(Number(root.controller.instant.moonAltitude || 0).toFixed(1)):qsTr("Moon: %1% illuminated · height %2° · %3° from the target").arg(Number(root.controller.instant.moonFraction || 0).toFixed(0)).arg(Number(root.controller.instant.moonAltitude || 0).toFixed(1)).arg(Number(root.controller.instant.moonSeparation || 0).toFixed(1));color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                        SettingLabel {text:qsTr("Day, twilight and night");explanation:qsTr("Sunrise and sunset use the Sun's upper edge with standard refraction at your elevation, over a flat horizon. Twilight and astronomical night use the geometric centre of the Sun at −6° and −18°. Mountains, buildings and actual weather can change the observed times.")}
                        Repeater {model:root.controller.result.daylightSlots || []
                            Label {required property var modelData;text:qsTr("Daylight: %1 → %2").arg(modelData.start).arg(modelData.end);color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                        }
                        Repeater {model:root.controller.result.nightSlots || []
                            Label {required property var modelData;text:qsTr("Astronomical night: %1 → %2").arg(modelData.start).arg(modelData.end);color:Atelier.accent;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                        }
                        Repeater {model:root.controller.result.solarEvents || []
                            Label {required property var modelData;text:modelData.name+" · "+modelData.time;color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                        }
                        Label {text:qsTr("Geometric visibility only. Check weather, obstacles and telescope availability separately.");color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12;Layout.bottomMargin:20}
                    }
                }
            }
            Rectangle {width:1;Layout.fillHeight:true;color:Atelier.border}
            Rectangle {
                Layout.preferredWidth:root.width<1100?300:340;Layout.fillHeight:true;color:Atelier.surface
                ScrollView {
                    id:inspector;anchors.fill:parent;anchors.margins:16;contentWidth:availableWidth;clip:true
                    ColumnLayout {
                        width:inspector.availableWidth;spacing:14
                        SettingLabel {text:qsTr("1 · Which object?");explanation:qsTr("Search the offline OpenNGC catalogue by NGC, IC or Messier number. Moon and planets use date-dependent positions. CDS Sesame can resolve more names online; only the name is sent.")}
                        ObservationField {id:query;objectName:"observationSearch";Layout.fillWidth:true;label:qsTr("Name or catalogue number");help:qsTr("Examples: Moon, Jupiter, M31, M42, NGC 7000. Start typing to filter the local catalogue.");onTextChanged:root.controller.search(text)}
                        DarkComboBox {objectName:"observationTarget";Layout.fillWidth:true;model:root.controller.searchResults;textRole:"name";displayText:query.text.length>0?qsTr("%1 results · choose an object").arg(root.controller.searchResults.length):(root.controller.result.target || "");explanation:qsTr("Open the list to choose a suggestion or a search result. The selected object appears above the curve.");onActivated:index=>{root.manual=false;root.controller.selectTarget(root.controller.searchResults[index].id)}}
                        RowLayout {Layout.fillWidth:true;spacing:8
                            IconButton {objectName:"observationFavoriteTarget";text:root.controller.targetFavorite?"★":"☆";Layout.preferredWidth:36;Accessible.name:root.controller.targetFavorite?qsTr("★ Remove object from favourites"):qsTr("☆ Save object to favourites");onClicked:root.controller.toggleFavoriteTarget();DarkToolTip {visible:parent.hovered || parent.activeFocus;text:parent.Accessible.name}}
                            DarkComboBox {Layout.fillWidth:true;enabled:root.controller.favoriteTargets.length>0;model:root.controller.favoriteTargets;textRole:"name";displayText:qsTr("Favourite objects · %1").arg(root.controller.favoriteTargets.length);explanation:qsTr("Choose a saved object. To remove it, select it and use the favourite button.");onActivated:index=>{root.manual=false;root.controller.selectTarget(root.controller.favoriteTargets[index].id)}}
                        }
                        IconButton {text:root.controller.searching?qsTr("Searching…"):qsTr("Search other names online");Layout.fillWidth:true;enabled:!root.controller.searching && query.text.trim().length>0;onClicked:root.controller.searchOnline(query.text)}
                        Label {visible:root.controller.searchStatus.length>0;text:root.controller.searchStatus;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12}
                        SettingLabel {text:qsTr("2 · From where?");explanation:qsTr("Choose a city or enter coordinates in Advanced. Preset cities use approximate city centres at sea level; city search supplies its reported elevation. Save your actual observing site as a favourite.")}
                        DarkComboBox {id:sitePicker;objectName:"observationSite";Layout.fillWidth:true;model:root.controller.sites;textRole:"label";displayText:root.controller.settings.name || qsTr("Custom location");explanation:qsTr("The city's time zone follows local daylight-saving rules. A map click uses UTC until you choose a time zone in Advanced.");onActivated:index=>{root.manual=false;let s=Object.assign({},root.controller.settings,root.controller.sites[index]);root.controller.calculate(s)}}
                        RowLayout {Layout.fillWidth:true;spacing:8
                            IconButton {Layout.fillWidth:true;text:root.favoriteSiteIndex()>=0?qsTr("★ Favourite place"):qsTr("☆ Favourite place");Accessible.name:root.favoriteSiteIndex()>=0?qsTr("★ Favourite place · manage"):qsTr("☆ Save place to favourites");onClicked:{root.saveSiteOpen=!root.saveSiteOpen;favoriteName.text=root.controller.settings.name || "";}}
                            IconButton {Layout.fillWidth:true;text:qsTr("Another city…");onClicked:root.citySearchOpen=!root.citySearchOpen}
                        }
                        ObservationField {id:favoriteName;visible:root.saveSiteOpen;Layout.fillWidth:true;label:qsTr("Favourite site name");help:qsTr("Give your observing site a name. Coordinates and time zone are kept on this computer.")}
                        RowLayout {visible:root.saveSiteOpen;Layout.fillWidth:true;spacing:8
                            IconButton {Layout.fillWidth:true;text:qsTr("Save this site");enabled:favoriteName.text.trim().length>0;onClicked:{if(root.controller.saveSite(favoriteName.text))root.saveSiteOpen=false;}}
                            IconButton {Layout.fillWidth:true;text:qsTr("Remove favourite");visible:root.favoriteSiteIndex()>=0;onClicked:{root.controller.removeSite(root.favoriteSiteIndex());root.saveSiteOpen=false;}}
                        }
                        ObservationField {id:cityQuery;visible:root.citySearchOpen;Layout.fillWidth:true;label:qsTr("Another city");help:qsTr("Enter a city name, optionally followed by its country. Searching sends only this text to Open-Meteo / GeoNames. No weather is requested.")}
                        IconButton {visible:root.citySearchOpen;Layout.fillWidth:true;text:root.controller.locating?qsTr("Searching…"):qsTr("Find this city online");busy:root.controller.locating;enabled:!root.controller.locating && cityQuery.text.trim().length>=2;onClicked:root.controller.searchCity(cityQuery.text)}
                        Label {visible:root.citySearchOpen && root.controller.locationStatus.length>0;text:root.controller.locationStatus;color:Atelier.secondary;Layout.fillWidth:true;wrapMode:Text.Wrap;font.pixelSize:12}
                        Repeater {model:root.citySearchOpen?root.controller.cityResults:[]
                            IconButton {required property int index;required property var modelData;Layout.fillWidth:true;text:modelData.name;onClicked:{root.manual=false;root.controller.selectCity(index)}}
                        }
                        SettingLabel {text:qsTr("3 · Which night?");explanation:qsTr("The date is local to the selected time zone. The curve spans noon to the next noon, so the complete night stays together.")}
                        ObservationField {id:dateInput;objectName:"observationDate";Layout.fillWidth:true;label:qsTr("Local date · YYYY-MM-DD");help:qsTr("For example 2026-10-11. Dates from 1900 to 2100 are supported.");onAccepted:root.calculate()}
                        RowLayout {spacing:8;IconButton {text:qsTr("Previous day");onClicked:root.stepDay(-1)}IconButton {text:qsTr("Next day");onClicked:root.stepDay(1)}}
                        ParameterControl {id:minimum;objectName:"observationMinimum";Layout.fillWidth:true;label:qsTr("Minimum height · degrees");explanation:qsTr("30° is a useful starting point. Higher limits reduce the available time but avoid the thickest atmosphere near the horizon. Lowering this limit cannot remove buildings or mountains.");from:0;to:80;value:30;decimals:0;stepSize:1;onGestureEnded:root.calculate()}
                        DarkComboBox {id:darkness;objectName:"observationDarkness";Layout.fillWidth:true;model:[qsTr("Very dark · Sun below −18°"),qsTr("Dark · Sun below −12°"),qsTr("Twilight · Sun below −6°"),qsTr("Any sky brightness")];explanation:qsTr("Deep-sky objects benefit from a very dark sky. The Moon and bright planets can be observed in twilight, so that limit is suggested automatically. Any sky brightness also includes daytime; exercise particular care with objects near the Sun.");onActivated:root.calculate()}
                        Label {visible:root.controller.error.length>0;text:root.controller.error+"\n"+qsTr("The displayed results keep the previous valid choices.");color:Atelier.danger;Layout.fillWidth:true;wrapMode:Text.Wrap;Accessible.role:Accessible.AlertMessage}
                        InspectorFamily {
                            id:advanced;Layout.fillWidth:true;title:qsTr("Advanced · coordinates & sources");expanded:false
                            onActivated:expanded=!expanded
                            ColumnLayout {Layout.fillWidth:true;spacing:12
                                Label {text:qsTr("Set an exact observing site or J2000 celestial coordinates. The module calculates body centres without atmospheric refraction, terrain or a local horizon mask.");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                                ObservationField {id:latitude;objectName:"observationLatitude";Layout.fillWidth:true;label:qsTr("Latitude · degrees");help:qsTr("North is positive; south is negative. Range −90 to +90.");onAccepted:root.calculate()}
                                ObservationField {id:longitude;Layout.fillWidth:true;label:qsTr("Longitude · degrees");help:qsTr("East is positive; west is negative. Range −180 to +180.");onAccepted:root.calculate()}
                                ObservationField {id:elevation;Layout.fillWidth:true;label:qsTr("Elevation · metres");help:qsTr("Height above sea level, −500 to 10000 metres. Sunrise uses standard refraction adjusted for this elevation. Terrain and horizon dip are not modelled.");onAccepted:root.calculate()}
                                ObservationField {id:timezone;Layout.fillWidth:true;label:qsTr("Time zone");help:qsTr("Use an IANA name such as Europe/Paris, America/Santiago or UTC. Clock changes are applied automatically.");onAccepted:root.calculate()}
                                ObservationField {id:rightAscension;Layout.fillWidth:true;label:qsTr("Right ascension J2000 · degrees");help:qsTr("0 to less than 360 degrees. If your catalogue gives hours, multiply by 15.")}
                                ObservationField {id:declination;Layout.fillWidth:true;label:qsTr("Declination J2000 · degrees");help:qsTr("−90 to +90 degrees. Manual coordinates are for fixed objects, not the Moon or planets.")}
                                IconButton {text:qsTr("Use these celestial coordinates");onClicked:{root.manual=true;root.calculate()}}
                                Label {text:qsTr("Source: %1").arg(root.controller.result.source || "");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true}
                                Label {text:qsTr("OpenNGC: %1 objects with coordinates, plus Solar System targets. Positions include precession and nutation. Fixed objects omit proper motion and annual aberration. Astronomy Engine models moving bodies; planning precision is about one arcminute, not telescope pointing accuracy. Windows use one-minute bracketing; boundaries are refined to one second and displayed to the minute.").arg(root.controller.catalogCount-8);color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                                Label {text:(root.controller.result.startUtc || "")+" → "+(root.controller.result.endUtc || "");color:Atelier.secondary;wrapMode:Text.Wrap;Layout.fillWidth:true;font.pixelSize:12}
                            }
                        }
                    }
                }
            }
        }
    }
}
