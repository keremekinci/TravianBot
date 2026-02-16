import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

ApplicationWindow {
    id: win
    width: 1200
    height: 720
    visible: true
    title: "Travian Dashboard"

    property var modelObj: (typeof TravianModel !== "undefined") ? TravianModel : null
    property var allData: modelObj ? modelObj.allData : ({})
    property var villages: modelObj ? modelObj.villages : []
    property int selectedVillageIndex: 0

    function currentVillage() {
        if (!villages || villages.length === 0) return null
        if (selectedVillageIndex < 0 || selectedVillageIndex >= villages.length) return villages[0]
        return villages[selectedVillageIndex]
    }

    function vData() {
        var v = currentVillage()
        if (!v) return null
        return v.data ? v.data : null
    }

    function tribeToName(tribe) {
        if (tribe === "1" || tribe === 1) return "Roma"
        if (tribe === "2" || tribe === 2) return "Cermen"
        if (tribe === "3" || tribe === 3) return "Galya"
        if (tribe === "4" || tribe === 4) return "Dogu"
        if (tribe === "5" || tribe === 5) return "Hun"
        if (tribe === "6" || tribe === 6) return "Misir"
        return "?"
    }

    function gidToName(gid) {
        var g = String(gid)
        if (g === "1") return "Oduncu"
        if (g === "2") return "Tugla Ocagi"
        if (g === "3") return "Demir Madeni"
        if (g === "4") return "Tarla"
        if (g === "19") return "Kisla"
        if (g === "20") return "Ahir"
        if (g === "21") return "Atolye"
        if (g === "0") return "Bos Alan"
        return "Bina #" + g
    }

    function buildingDisplayName(b) {
        if (b.name && b.name !== "") return b.name
        return gidToName(b.gid)
    }

    function selectedVillage() {
        var v = currentVillage()
        return v ? v.id : 0
    }

    header: ToolBar {
        height: 52
        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            spacing: 12

            Label {
                property var d1: vData() && vData().dorf1 ? vData().dorf1 : ({})
                text: d1.villageName ? (d1.villageName + " | " + tribeToName(d1.tribe)) : "Travian Dashboard"
                font.pixelSize: 18
                font.bold: true
            }

            Rectangle { Layout.fillWidth: true; color: "transparent" }

            // Attack warning
            Rectangle {
                height: 36
                radius: 6
                color: "#ff5555"
                Layout.preferredWidth: attackWarningRow.implicitWidth + 20
                visible: {
                    if (!allData || !allData.villageListWithAttacks) return false
                    var total = 0
                    var list = allData.villageListWithAttacks
                    for (var i = 0; i < list.length; i++) {
                        total += (list[i].incomingAttacksAmount || 0)
                    }
                    return total > 0
                }

                RowLayout {
                    id: attackWarningRow
                    anchors.centerIn: parent
                    spacing: 8

                    Label {
                        text: "⚠️"
                        font.pixelSize: 18
                    }

                    Label {
                        text: {
                            if (!allData || !allData.villageListWithAttacks) return ""
                            var total = 0
                            var list = allData.villageListWithAttacks
                            for (var i = 0; i < list.length; i++) {
                                total += (list[i].incomingAttacksAmount || 0)
                            }
                            return total + " Saldırı Geliyor!"
                        }
                        color: "white"
                        font.pixelSize: 14
                        font.bold: true
                    }
                }
            }

            Label {
                text: modelObj ? modelObj.statusText : "Model yok"
                opacity: 0.7
            }

            Button {
                text: (modelObj && modelObj.loading) ? "Yukleniyor..." : "Yenile"
                enabled: modelObj ? !modelObj.loading : false
                onClicked: { if (modelObj) modelObj.startFetch() }
            }

            Button {
                text: "⚠️ Test"
                palette.buttonText: "red"
                onClicked: {
                    console.log("Testing notification...");
                    if (modelObj) {
                        modelObj.testNotification();
                    }
                }
            }

            Button {
                text: "ℹ️ Telegram"
                onClicked: telegramInfoDialog.open()
            }

        }
    }

    Item {
        anchors.fill: parent

        RowLayout {
            anchors.fill: parent
            anchors.margins: 10
            anchors.bottomMargin: 50
            spacing: 10

        // Sol %70: Köyler + TabView
        RowLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 10

            // Köy listesi
            Rectangle {
                Layout.preferredWidth: 220
                Layout.fillHeight: true
                radius: 12
                color: "#1f2430"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 12
                    spacing: 10

                    Label { text: "Koyler"; color: "white"; font.pixelSize: 16; font.bold: true }

                    ListView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        model: villages
                        clip: true
                        spacing: 6

                        delegate: Rectangle {
                            width: ListView.view.width
                            height: 44
                            radius: 8
                            color: (index === selectedVillageIndex) ? "#3a7bd5" : "#2a3142"

                            // Get attack info for this village
                            function getAttackInfo() {
                                if (!allData || !allData.villageListWithAttacks) return null
                                var list = allData.villageListWithAttacks
                                for (var i = 0; i < list.length; i++) {
                                    if (list[i].id === modelData.id) {
                                        return list[i]
                                    }
                                }
                                return null
                            }

                            property var attackInfo: getAttackInfo()
                            property int attackCount: attackInfo ? (attackInfo.incomingAttacksAmount || 0) : 0

                            RowLayout {
                                anchors.fill: parent
                                anchors.margins: 8

                                Label {
                                    text: modelData.name
                                    color: "white"
                                    font.pixelSize: 13
                                    Layout.fillWidth: true
                                    elide: Text.ElideRight
                                }

                                // Attack indicator
                                Rectangle {
                                    visible: attackCount > 0
                                    width: 24
                                    height: 24
                                    radius: 12
                                    color: "#ff5555"
                                    Label {
                                        anchors.centerIn: parent
                                        text: attackCount
                                        color: "white"
                                        font.pixelSize: 10
                                        font.bold: true
                                    }
                                }

                                Label {
                                    text: "#" + modelData.id
                                    color: "white"
                                    opacity: 0.6
                                    font.pixelSize: 11
                                }
                            }

                            MouseArea {
                                anchors.fill: parent
                                onClicked: selectedVillageIndex = index
                            }
                        }
                    }
                }
            }

            // TabView
            Rectangle {
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 12
                color: "#121621"

                ColumnLayout {
                    anchors.fill: parent
                    anchors.margins: 0
                    spacing: 0

                    // Tab butonlari
                    Rectangle {
                        Layout.fillWidth: true
                        height: 50
                        color: "#1a1f2a"
                        radius: 12

                        RowLayout {
                            anchors.fill: parent
                            anchors.margins: 8
                            spacing: 4

                            Repeater {
                                model: ["Genel Bakis", "Tarlalar", "Binalar", "Askeri", "Yagma"]

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.fillHeight: true
                                    radius: 8
                                    color: tabStack.currentIndex === index ? "#3a7bd5" : "transparent"

                                    Label {
                                        anchors.centerIn: parent
                                        text: modelData
                                        color: "white"
                                        font.pixelSize: 13
                                    }

                                    MouseArea {
                                        anchors.fill: parent
                                        onClicked: tabStack.currentIndex = index
                                    }
                                }
                            }
                        }
                    }

                    // Tab icerigi
                    StackLayout {
                        id: tabStack
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        currentIndex: 0

                    // === TAB 0: Genel Bakis ===
                    Flickable {
                        clip: true
                        contentHeight: overviewCol.implicitHeight + 20

                        ColumnLayout {
                            id: overviewCol
                            width: parent.width
                            spacing: 12

                            property var d1: vData() && vData().dorf1 ? vData().dorf1 : ({})
                            property var d2: vData() && vData().dorf2 ? vData().dorf2 : ({})

                            // Kaynaklar
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                height: 140
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    Label { text: "Mevcut Kaynaklar"; color: "white"; font.pixelSize: 15; font.bold: true }

                                    GridLayout {
                                        columns: 4
                                        rowSpacing: 6
                                        columnSpacing: 20

                                        Label { text: "Odun:"; color: "#aab" }
                                        Label { text: overviewCol.d1.lumber || "0"; color: "white" }
                                        Label { text: "Tugla:"; color: "#aab" }
                                        Label { text: overviewCol.d1.clay || "0"; color: "white" }

                                        Label { text: "Demir:"; color: "#aab" }
                                        Label { text: overviewCol.d1.iron || "0"; color: "white" }
                                        Label { text: "Tahil:"; color: "#aab" }
                                        Label { text: overviewCol.d1.crop || "0"; color: "white" }
                                    }

                                    RowLayout {
                                        spacing: 20
                                        Label { text: "Depo: " + (overviewCol.d1.warehouseCapacity || "-"); color: "#888" }
                                        Label { text: "Ambar: " + (overviewCol.d1.granaryCapacity || "-"); color: "#888" }
                                    }
                                }
                            }

                            // Uretim
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: 100
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    Label { text: "Saatlik Uretim"; color: "white"; font.pixelSize: 15; font.bold: true }

                                    RowLayout {
                                        spacing: 30
                                        Label { text: "Odun: " + (overviewCol.d1.productionLumber || "0") + "/s"; color: "#4CAF50" }
                                        Label { text: "Tugla: " + (overviewCol.d1.productionClay || "0") + "/s"; color: "#FF9800" }
                                        Label { text: "Demir: " + (overviewCol.d1.productionIron || "0") + "/s"; color: "#9E9E9E" }
                                        Label { text: "Tahil: " + (overviewCol.d1.productionCrop || "0") + "/s"; color: "#FFEB3B" }
                                    }
                                }
                            }

                            // Insaat Kuyrugu
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: 120
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    property var queue: overviewCol.d1.constructionQueue || []

                                    Label { text: "Insaat Kuyrugu"; color: "white"; font.pixelSize: 15; font.bold: true }

                                    Label {
                                        visible: parent.queue.length === 0
                                        text: "Kuyruk bos"
                                        color: "#666"
                                    }

                                    Repeater {
                                        model: parent.queue

                                        RowLayout {
                                            spacing: 10
                                            Label { text: modelData.buildingName || "Bina"; color: "white" }
                                            Label { text: "-> Lv " + (modelData.level || "?"); color: "#4CAF50" }
                                            Label { text: modelData.remainingTime || ""; color: "#FF9800" }
                                        }
                                    }
                                }
                            }

                            // Gelen Saldırılar
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: attackInfoCol.implicitHeight + 28
                                radius: 10
                                color: "#3d1f1f"
                                border.color: "#ff5555"
                                border.width: 2

                                property var villageAttackInfo: {
                                    if (!allData || !allData.villageListWithAttacks) return null
                                    var vid = selectedVillage()
                                    var list = allData.villageListWithAttacks
                                    for (var i = 0; i < list.length; i++) {
                                        if (list[i].id === vid) {
                                            return list[i]
                                        }
                                    }
                                    return null
                                }

                                visible: villageAttackInfo !== null && villageAttackInfo.incomingAttacksAmount > 0

                                ColumnLayout {
                                    id: attackInfoCol
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 12

                                    RowLayout {
                                        spacing: 10
                                        Label {
                                            text: "⚠️ GELEN SALDIRILAR"
                                            color: "#ff5555"
                                            font.pixelSize: 15
                                            font.bold: true
                                        }
                                        Rectangle {
                                            width: 32
                                            height: 32
                                            radius: 16
                                            color: "#ff5555"
                                            Label {
                                                anchors.centerIn: parent
                                                text: {
                                                    var info = villageAttackInfo
                                                    return info ? info.incomingAttacksAmount : 0
                                                }
                                                color: "white"
                                                font.pixelSize: 14
                                                font.bold: true
                                            }
                                        }
                                    }

                                    Label {
                                        text: {
                                            var info = villageAttackInfo
                                            if (!info) return ""
                                            var count = info.incomingAttacksAmount || 0
                                            return count + " adet saldırı bu köye geliyor!"
                                        }
                                        color: "#ffaaaa"
                                        font.pixelSize: 13
                                        wrapMode: Text.WordWrap
                                        Layout.fillWidth: true
                                    }

                                    // Attack type indicators
                                    RowLayout {
                                        spacing: 15
                                        visible: {
                                            var info = villageAttackInfo
                                            if (!info || !info.incomingAttacksSymbols) return false
                                            var symbols = info.incomingAttacksSymbols
                                            return (symbols.red > 0 || symbols.yellow > 0 || symbols.green > 0 || symbols.gray > 0)
                                        }

                                        Repeater {
                                            model: {
                                                var info = villageAttackInfo
                                                if (!info || !info.incomingAttacksSymbols) return []
                                                var symbols = info.incomingAttacksSymbols
                                                var result = []
                                                if (symbols.red > 0) result.push({color: "#ff5555", count: symbols.red, label: "Normal Saldırı"})
                                                if (symbols.yellow > 0) result.push({color: "#ffeb3b", count: symbols.yellow, label: "Yağma"})
                                                if (symbols.green > 0) result.push({color: "#4caf50", count: symbols.green, label: "Destek"})
                                                if (symbols.gray > 0) result.push({color: "#888888", count: symbols.gray, label: "Diğer"})
                                                return result
                                            }

                                            RowLayout {
                                                spacing: 6
                                                Rectangle {
                                                    width: 12
                                                    height: 12
                                                    radius: 6
                                                    color: modelData.color
                                                }
                                                Label {
                                                    text: modelData.label + ": " + modelData.count
                                                    color: "#ddd"
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }
                                    }

                                    // Attack details from rally point
                                    Repeater {
                                        model: {
                                            var vid = selectedVillage()
                                            if (!modelObj || !modelObj.attackDetails) return []
                                            return modelObj.attackDetails[vid.toString()] || []
                                        }

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: attackDetailCol.implicitHeight + 16
                                            radius: 6
                                            color: "#2a1515"
                                            border.color: "#ff5555"
                                            border.width: 1

                                            ColumnLayout {
                                                id: attackDetailCol
                                                anchors.fill: parent
                                                anchors.margins: 8
                                                spacing: 6

                                                RowLayout {
                                                    spacing: 8
                                                    Label {
                                                        text: {
                                                            var type = modelData.type || "unknown"
                                                            var typeLabel = {
                                                                "attack": "⚔️ Normal Saldırı",
                                                                "raid": "🏹 Yağma",
                                                                "support": "🛡️ Destek",
                                                                "reinforcement": "👥 Takviye"
                                                            }
                                                            return typeLabel[type] || "❓ " + type
                                                        }
                                                        color: "#ff8888"
                                                        font.pixelSize: 12
                                                        font.bold: true
                                                    }
                                                    Rectangle {
                                                        width: 4
                                                        height: 4
                                                        radius: 2
                                                        color: "#666"
                                                    }
                                                    Label {
                                                        text: "ID: " + (modelData.id || "?")
                                                        color: "#888"
                                                        font.pixelSize: 10
                                                    }
                                                }

                                                // Countdown timer - only show if we have timing data
                                                Label {
                                                    visible: modelData.remainingSeconds > 0 || modelData.arrivalDateTime !== "Bilinmiyor"
                                                    text: {
                                                        var remaining = modelData.remainingSeconds || 0
                                                        if (remaining === 0) return "⏱️ Süre bilinmiyor - Rally Point'e bakın"
                                                        var hours = Math.floor(remaining / 3600)
                                                        var minutes = Math.floor((remaining % 3600) / 60)
                                                        var seconds = remaining % 60
                                                        return "⏱️ " + hours + " saat " + minutes + " dakika " + seconds + " saniye"
                                                    }
                                                    color: modelData.remainingSeconds > 0 ? "#ffcc00" : "#888"
                                                    font.pixelSize: 13
                                                    font.bold: modelData.remainingSeconds > 0
                                                }

                                                // Arrival time - only show if we have it
                                                Label {
                                                    visible: modelData.arrivalDateTime && modelData.arrivalDateTime !== "Bilinmiyor"
                                                    text: "🕐 Varış: " + (modelData.arrivalDateTime || "")
                                                    color: "#aaa"
                                                    font.pixelSize: 11
                                                }

                                                // Info message when timing is unknown
                                                Label {
                                                    visible: (!modelData.arrivalDateTime || modelData.arrivalDateTime === "Bilinmiyor") && modelData.displayName
                                                    text: "ℹ️ Detaylı bilgi için Rally Point'i açın"
                                                    color: "#888"
                                                    font.pixelSize: 10
                                                    font.italic: true
                                                }

                                                // Origin village (if available)
                                                Label {
                                                    visible: modelData.originVillage !== undefined
                                                    text: "📍 Kaynak: " + (modelData.originVillage ? JSON.stringify(modelData.originVillage) : "?")
                                                    color: "#999"
                                                    font.pixelSize: 10
                                                    elide: Text.ElideRight
                                                    Layout.fillWidth: true
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        visible: {
                                            var vid = selectedVillage()
                                            if (!modelObj || !modelObj.attackDetails) return true
                                            var attacks = modelObj.attackDetails[vid.toString()] || []
                                            return attacks.length === 0
                                        }
                                        text: "⏰ Saldırı detayları yükleniyor..."
                                        color: "#aaa"
                                        font.pixelSize: 11
                                        font.italic: true
                                    }
                                }
                            }

                            // Asker Ozeti
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: overviewTroopsCol.implicitHeight + 28
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    id: overviewTroopsCol
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    property var troops: overviewCol.d1.troops || []

                                    Label { text: "Koydeki Askerler"; color: "white"; font.pixelSize: 15; font.bold: true }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: overviewTroopsCol.troops

                                            Rectangle {
                                                width: 100
                                                height: 28
                                                radius: 4
                                                color: "#2a3142"

                                                Row {
                                                    anchors.centerIn: parent
                                                    spacing: 6
                                                    Label { text: modelData.count || "0"; color: "#4CAF50"; font.bold: true; font.pixelSize: 12 }
                                                    Label { text: modelData.displayName || "?"; color: "white"; font.pixelSize: 11 }
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        visible: overviewTroopsCol.troops.length === 0
                                        text: "Asker yok"
                                        color: "#666"
                                    }
                                }
                            }

                            // Askeri Binalar Ozet
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: 80
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 14
                                    spacing: 8

                                    property var buildings: overviewCol.d2.buildings || []

                                    function getMilitary() {
                                        var result = []
                                        for (var i = 0; i < buildings.length; i++) {
                                            var g = String(buildings[i].gid)
                                            if (g === "19" || g === "20" || g === "21") {
                                                result.push(buildings[i])
                                            }
                                        }
                                        return result
                                    }

                                    Label { text: "Askeri Binalar"; color: "white"; font.pixelSize: 15; font.bold: true }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: parent.parent.getMilitary()

                                            Rectangle {
                                                width: 80
                                                height: 28
                                                radius: 4
                                                color: "#2a3142"

                                                Label {
                                                    anchors.centerIn: parent
                                                    text: gidToName(modelData.gid)
                                                    color: "white"
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }
                                    }

                                    Label {
                                        visible: parent.getMilitary().length === 0
                                        text: "Askeri bina yok"
                                        color: "#666"
                                    }
                                }
                            }
                        }
                    }

                    // === TAB 1: Tarlalar ===
                    Flickable {
                        clip: true
                        contentHeight: fieldsCol.implicitHeight + 20

                        ColumnLayout {
                            id: fieldsCol
                            width: parent.width
                            spacing: 12

                            property var d1: vData() && vData().dorf1 ? vData().dorf1 : ({})
                            property var allFields: d1.resourceFields || []
                            
                            // Filter and group fields by type
                            function getFieldsByType(gid) {
                                var result = []
                                for (var i = 0; i < allFields.length; i++) {
                                    if (String(allFields[i].gid) === String(gid)) {
                                        result.push(allFields[i])
                                    }
                                }
                                return result
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                height: 50
                                radius: 10
                                color: "#1f2430"

                                Label {
                                    anchors.centerIn: parent
                                    text: "Kaynak Alanları"
                                    color: "white"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                            }

                            // Odun (gid: 1)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: woodCol.implicitHeight + 20
                                radius: 10
                                color: "#1f2430"
                                visible: fieldsCol.getFieldsByType("1").length > 0

                                ColumnLayout {
                                    id: woodCol
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 6

                                    Label {
                                        text: "🪵 Oduncu (" + fieldsCol.getFieldsByType("1").length + ")"
                                        color: "#8BC34A"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: fieldsCol.getFieldsByType("1")

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 50
                                            radius: 6
                                            color: "#2a3142"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10

                                                Label {
                                                    text: buildingDisplayName(modelData)
                                                    color: "white"
                                                    Layout.fillWidth: true
                                                }

                                                Label {
                                                    text: "Seviye " + (modelData.level || "?")
                                                    color: "#4CAF50"
                                                    font.bold: true
                                                    Layout.preferredWidth: 70
                                                }

                                                Label {
                                                    text: "Hedef:"
                                                    color: "#888"
                                                    font.pixelSize: 11
                                                }

                                                SpinBox {
                                                    id: woodTargetSpin
                                                    property int currentLevel: parseInt(modelData.level) || 0
                                                    from: currentLevel + 1
                                                    to: 20
                                                    value: currentLevel + 1
                                                    stepSize: 1
                                                    Layout.preferredWidth: 100
                                                    editable: true
                                                }

                                                Rectangle {
                                                    width: 100
                                                    height: 32
                                                    radius: 4
                                                    color: woodQueueMa.pressed ? "#1565C0" : (woodQueueMa.containsMouse ? "#1976D2" : "#2196F3")

                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: "📋 Kuyruğa"
                                                        color: "white"
                                                        font.pixelSize: 11
                                                    }

                                                    MouseArea {
                                                        id: woodQueueMa
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var vid = selectedVillage()
                                                            var slot = modelData.slotId || modelData.aid
                                                            var bName = buildingDisplayName(modelData)
                                                            if (vid && slot && modelObj) {
                                                                modelObj.addToBuildQueue(vid, parseInt(slot), woodTargetSpin.value, bName)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Tuğla (gid: 2)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: clayCol.implicitHeight + 20
                                radius: 10
                                color: "#1f2430"
                                visible: fieldsCol.getFieldsByType("2").length > 0

                                ColumnLayout {
                                    id: clayCol
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 6

                                    Label {
                                        text: "🧱 Tuğla Ocağı (" + fieldsCol.getFieldsByType("2").length + ")"
                                        color: "#FF9800"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: fieldsCol.getFieldsByType("2")

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 50
                                            radius: 6
                                            color: "#2a3142"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10

                                                Label {
                                                    text: buildingDisplayName(modelData)
                                                    color: "white"
                                                    Layout.fillWidth: true
                                                }

                                                Label {
                                                    text: "Seviye " + (modelData.level || "?")
                                                    color: "#4CAF50"
                                                    font.bold: true
                                                    Layout.preferredWidth: 70
                                                }

                                                Label {
                                                    text: "Hedef:"
                                                    color: "#888"
                                                    font.pixelSize: 11
                                                }

                                                SpinBox {
                                                    id: clayTargetSpin
                                                    property int currentLevel: parseInt(modelData.level) || 0
                                                    from: currentLevel + 1
                                                    to: 20
                                                    value: currentLevel + 1
                                                    stepSize: 1
                                                    Layout.preferredWidth: 100
                                                    editable: true
                                                }

                                                Rectangle {
                                                    width: 100
                                                    height: 32
                                                    radius: 4
                                                    color: clayQueueMa.pressed ? "#1565C0" : (clayQueueMa.containsMouse ? "#1976D2" : "#2196F3")

                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: "📋 Kuyruğa"
                                                        color: "white"
                                                        font.pixelSize: 11
                                                    }

                                                    MouseArea {
                                                        id: clayQueueMa
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var vid = selectedVillage()
                                                            var slot = modelData.slotId || modelData.aid
                                                            var bName = buildingDisplayName(modelData)
                                                            if (vid && slot && modelObj) {
                                                                modelObj.addToBuildQueue(vid, parseInt(slot), clayTargetSpin.value, bName)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Demir (gid: 3)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: ironCol.implicitHeight + 20
                                radius: 10
                                color: "#1f2430"
                                visible: fieldsCol.getFieldsByType("3").length > 0

                                ColumnLayout {
                                    id: ironCol
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 6

                                    Label {
                                        text: "⚙️ Demir Madeni (" + fieldsCol.getFieldsByType("3").length + ")"
                                        color: "#9E9E9E"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: fieldsCol.getFieldsByType("3")

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 50
                                            radius: 6
                                            color: "#2a3142"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10

                                                Label {
                                                    text: buildingDisplayName(modelData)
                                                    color: "white"
                                                    Layout.fillWidth: true
                                                }

                                                Label {
                                                    text: "Seviye " + (modelData.level || "?")
                                                    color: "#4CAF50"
                                                    font.bold: true
                                                    Layout.preferredWidth: 70
                                                }

                                                Label {
                                                    text: "Hedef:"
                                                    color: "#888"
                                                    font.pixelSize: 11
                                                }

                                                SpinBox {
                                                    id: ironTargetSpin
                                                    property int currentLevel: parseInt(modelData.level) || 0
                                                    from: currentLevel + 1
                                                    to: 20
                                                    value: currentLevel + 1
                                                    stepSize: 1
                                                    Layout.preferredWidth: 100
                                                    editable: true
                                                }

                                                Rectangle {
                                                    width: 100
                                                    height: 32
                                                    radius: 4
                                                    color: ironQueueMa.pressed ? "#1565C0" : (ironQueueMa.containsMouse ? "#1976D2" : "#2196F3")

                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: "📋 Kuyruğa"
                                                        color: "white"
                                                        font.pixelSize: 11
                                                    }

                                                    MouseArea {
                                                        id: ironQueueMa
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var vid = selectedVillage()
                                                            var slot = modelData.slotId || modelData.aid
                                                            var bName = buildingDisplayName(modelData)
                                                            if (vid && slot && modelObj) {
                                                                modelObj.addToBuildQueue(vid, parseInt(slot), ironTargetSpin.value, bName)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Tahıl (gid: 4)
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                Layout.topMargin: 0
                                height: cropCol.implicitHeight + 20
                                radius: 10
                                color: "#1f2430"
                                visible: fieldsCol.getFieldsByType("4").length > 0

                                ColumnLayout {
                                    id: cropCol
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 6

                                    Label {
                                        text: "🌾 Tarla (" + fieldsCol.getFieldsByType("4").length + ")"
                                        color: "#FFEB3B"
                                        font.pixelSize: 14
                                        font.bold: true
                                    }

                                    Repeater {
                                        model: fieldsCol.getFieldsByType("4")

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: 50
                                            radius: 6
                                            color: "#2a3142"

                                            RowLayout {
                                                anchors.fill: parent
                                                anchors.margins: 10
                                                spacing: 10

                                                Label {
                                                    text: buildingDisplayName(modelData)
                                                    color: "white"
                                                    Layout.fillWidth: true
                                                }

                                                Label {
                                                    text: "Seviye " + (modelData.level || "?")
                                                    color: "#4CAF50"
                                                    font.bold: true
                                                    Layout.preferredWidth: 70
                                                }

                                                Label {
                                                    text: "Hedef:"
                                                    color: "#888"
                                                    font.pixelSize: 11
                                                }

                                                SpinBox {
                                                    id: cropTargetSpin
                                                    property int currentLevel: parseInt(modelData.level) || 0
                                                    from: currentLevel + 1
                                                    to: 20
                                                    value: currentLevel + 1
                                                    stepSize: 1
                                                    Layout.preferredWidth: 100
                                                    editable: true
                                                }

                                                Rectangle {
                                                    width: 100
                                                    height: 32
                                                    radius: 4
                                                    color: cropQueueMa.pressed ? "#1565C0" : (cropQueueMa.containsMouse ? "#1976D2" : "#2196F3")

                                                    Label {
                                                        anchors.centerIn: parent
                                                        text: "📋 Kuyruğa"
                                                        color: "white"
                                                        font.pixelSize: 11
                                                    }

                                                    MouseArea {
                                                        id: cropQueueMa
                                                        anchors.fill: parent
                                                        hoverEnabled: true
                                                        cursorShape: Qt.PointingHandCursor
                                                        onClicked: {
                                                            var vid = selectedVillage()
                                                            var slot = modelData.slotId || modelData.aid
                                                            var bName = buildingDisplayName(modelData)
                                                            if (vid && slot && modelObj) {
                                                                modelObj.addToBuildQueue(vid, parseInt(slot), cropTargetSpin.value, bName)
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // === TAB 2: Binalar ===
                    Flickable {
                        clip: true
                        contentHeight: buildingsCol.implicitHeight + 20

                        ColumnLayout {
                            id: buildingsCol
                            width: parent.width
                            spacing: 8

                            property var d2: vData() && vData().dorf2 ? vData().dorf2 : ({})
                            property var allBuildings: d2.buildings || []
                            
                            // Filter out empty slots and merge with level data
                            function getNonEmptyBuildings() {
                                var result = []
                                for (var i = 0; i < allBuildings.length; i++) {
                                    var building = allBuildings[i]
                                    var gid = String(building.gid)
                                    if (gid !== "0") {
                                        // Level now comes directly from buildings array
                                        var level = parseInt(building.level) || 0
                                        // Create merged object
                                        var merged = {
                                            "slotId": building.slotId,
                                            "gid": building.gid,
                                            "name": building.name,
                                            "level": level
                                        }
                                        result.push(merged)
                                    }
                                }
                                return result
                            }

                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 12
                                height: 50
                                radius: 10
                                color: "#1f2430"

                                Label {
                                    anchors.centerIn: parent
                                    text: "Köy Binaları (" + buildingsCol.getNonEmptyBuildings().length + ")"
                                    color: "white"
                                    font.pixelSize: 16
                                    font.bold: true
                                }
                            }

                            Repeater {
                                model: buildingsCol.getNonEmptyBuildings()

                                Rectangle {
                                    Layout.fillWidth: true
                                    Layout.margins: 12
                                    Layout.topMargin: 0
                                    height: 50
                                    radius: 8
                                    color: "#1f2430"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 10
                                        spacing: 10

                                        Label {
                                            text: buildingDisplayName(modelData)
                                            color: "white"
                                            Layout.fillWidth: true
                                            font.pixelSize: 13
                                        }

                                        Label {
                                            text: "Seviye " + (modelData.level || "?")
                                            color: "#4CAF50"
                                            font.bold: true
                                            Layout.preferredWidth: 70
                                        }

                                        Label {
                                            text: "Hedef:"
                                            color: "#888"
                                            font.pixelSize: 11
                                        }

                                        SpinBox {
                                            id: buildingTargetSpin
                                            property int currentLevel: parseInt(modelData.level) || 0
                                            from: currentLevel + 1
                                            to: 20
                                            value: currentLevel + 1
                                            stepSize: 1
                                            Layout.preferredWidth: 100
                                            editable: true
                                        }

                                        Rectangle {
                                            width: 100
                                            height: 32
                                            radius: 4
                                            color: buildingQueueMa.pressed ? "#1565C0" : (buildingQueueMa.containsMouse ? "#1976D2" : "#2196F3")

                                            Label {
                                                anchors.centerIn: parent
                                                text: "📋 Kuyruğa"
                                                color: "white"
                                                font.pixelSize: 11
                                            }

                                            MouseArea {
                                                id: buildingQueueMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    var vid = selectedVillage()
                                                    var slot = modelData.slotId
                                                    var bName = buildingDisplayName(modelData)
                                                    if (vid && slot && modelObj) {
                                                        modelObj.addToBuildQueue(vid, parseInt(slot), buildingTargetSpin.value, bName)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // === TAB 3: Askeri ===
                    Flickable {
                        clip: true
                        contentHeight: militaryCol.implicitHeight + 40

                        ColumnLayout {
                            id: militaryCol
                            width: parent.width - 24
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 8

                            property var d1: vData() && vData().dorf1 ? vData().dorf1 : ({})
                            property var d2: vData() && vData().dorf2 ? vData().dorf2 : ({})
                            property var barracksData: vData() && vData().barracks ? vData().barracks : ({})
                            property var stableData: vData() && vData().stable ? vData().stable : ({})
                            property var workshopData: vData() && vData().workshop ? vData().workshop : ({})
                            property var allBuildings: d2.buildings || []
                            property var troops: d1.troops || []
                            property var trainableTroops: barracksData.trainableTroops || []
                            property var lockedTroops: barracksData.lockedTroops || []
                            property int tribe: d1.tribe ? parseInt(d1.tribe) : 1

                            // Roma askerleri (tribe 1)
                            // ======================= TRIBE TROOPS =======================
                            // tribe id: 1 Roma, 2 Cermen, 3 Galya, 4 Doğu, 5 Hun, 6 Mısır
                            property var troopsByTribe: ({
                                1: [ // ROMA
                                    {id: "u1",  name: "Lejyoner",             building: "barracks"},
                                    {id: "u2",  name: "Praetorian",           building: "barracks"},
                                    {id: "u3",  name: "Emperyan",             building: "barracks"},
                                    {id: "u4",  name: "Equites Legati",       building: "stable"},
                                    {id: "u5",  name: "Equites Imperatoris",  building: "stable"},
                                    {id: "u6",  name: "Equites Caesaris",     building: "stable"},
                                    {id: "u7",  name: "Koç Başı",             building: "workshop"},
                                    {id: "u8",  name: "Ateş Mancınığı",       building: "workshop"},
                                    {id: "u9",  name: "Senatör",              building: "residence"},
                                    {id: "u10", name: "Göçmen",               building: "residence"}
                                ],

                                2: [ // CERMEN (Teuton)
                                    {id: "u11", name: "Tokmak Sallayan",      building: "barracks"},
                                    {id: "u12", name: "Mızrakçı",             building: "barracks"},
                                    {id: "u13", name: "Balta Sallayan",       building: "barracks"},
                                    {id: "u14", name: "Casus",                building: "barracks"}, // Cermen casusu kışlada
                                    {id: "u15", name: "Paladin",              building: "stable"},
                                    {id: "u16", name: "Toyton Şövalyesi",     building: "stable"},
                                    {id: "u17", name: "Koç Başı",             building: "workshop"},
                                    {id: "u18", name: "Mancınık",             building: "workshop"},
                                    {id: "u19", name: "Reis",                 building: "residence"},
                                    {id: "u20", name: "Göçmen",               building: "residence"}
                                ],

                                3: [ // GALYA (Gaul)
                                    {id: "u21", name: "Phalanx",              building: "barracks"},
                                    {id: "u22", name: "Kılıçlı",              building: "barracks"},
                                    {id: "u23", name: "Casus",                building: "stable"},   // Galya casusu ahırda
                                    {id: "u24", name: "Toytatın Şimşeği",     building: "stable"},
                                    {id: "u25", name: "Druyid",               building: "stable"},
                                    {id: "u26", name: "Haeduan",              building: "stable"},
                                    {id: "u27", name: "Koç Başı",             building: "workshop"},
                                    {id: "u28", name: "Mancınık",             building: "workshop"},
                                    {id: "u29", name: "Kabile Reisi",         building: "residence"},
                                    {id: "u30", name: "Göçmen",               building: "residence"}
                                ],

                                5: [ // HUN
                                    {id: "u51", name: "Paralı Asker",         building: "barracks"},
                                    {id: "u52", name: "Okçu",                 building: "barracks"},
                                    {id: "u53", name: "Akıncı",               building: "barracks"}, 
                                    {id: "u54", name: "Keşifçi",              building: "stable"},
                                    {id: "u55", name: "Step Binicisi",        building: "stable"},
                                    {id: "u56", name: "Nişancı",              building: "stable"},
                                    {id: "u57", name: "Marauder",             building: "stable"},
                                    {id: "u58", name: "Koç Başı",             building: "workshop"},
                                    {id: "u59", name: "Mancınık",             building: "workshop"},
                                    {id: "u60", name: "Logades",              building: "residence"},
                                    {id: "u61", name: "Göçmen",               building: "residence"}
                                ],

                                6: [ // MISIR
                                    {id: "u61", name: "Köle Milisi",          building: "barracks"},
                                    {id: "u62", name: "Kül Bekçisi",          building: "barracks"},
                                    {id: "u63", name: "Khopesh Savaşçısı",    building: "barracks"},
                                    {id: "u64", name: "Sopdu Kaşifi",         building: "stable"},
                                    {id: "u65", name: "Anhur Muhafızı",       building: "stable"},
                                    {id: "u66", name: "Resheph Arabası",      building: "stable"},
                                    {id: "u67", name: "Koç Başı",             building: "workshop"},
                                    {id: "u68", name: "Taş Mancınığı",        building: "workshop"},
                                    {id: "u69", name: "Nomarch",              building: "residence"},
                                    {id: "u70", name: "Göçmen",               building: "residence"}
                                ]
                            })

                            property var tribeTroops: troopsByTribe[tribe] ? troopsByTribe[tribe] : troopsByTribe[1]

                            function getMilitaryBuildings() {
                                var result = []
                                for (var i = 0; i < allBuildings.length; i++) {
                                    var g = String(allBuildings[i].gid)
                                    if (g === "19" || g === "20" || g === "21") {
                                        result.push(allBuildings[i])
                                    }
                                }
                                return result
                            }

                            function getTroopCount(troopId) {
                                // backend'den dorf1.php parser unitClass olarak u11, u12 dönüyor
                                for (var i = 0; i < troops.length; i++) {
                                    if (troops[i].unitClass === troopId) {
                                        return parseInt(troops[i].count) || 0
                                    }
                                }
                                return 0
                            }

                            function globalToRelativeId(globalId) {
                                if (!globalId.startsWith("u")) return globalId
                                var gid = parseInt(globalId.replace("u", ""))
                                var rid = (gid - 1) % 10 + 1
                                return "t" + rid
                            }

                            function isTroopUnlocked(troopId) {
                                // Önce hangi binaya ait olduğunu bulalım
                                var buildingType = "barracks" 
                                var troopsList = tribeTroops
                                for (var t = 0; t < troopsList.length; t++) {
                                    if (troopsList[t].id === troopId) {
                                        buildingType = troopsList[t].building
                                        break
                                    }
                                }

                                var sourceList = []
                                if (buildingType === "barracks") sourceList = trainableTroops
                                else if (buildingType === "stable") sourceList = stableData.trainableTroops || []
                                else if (buildingType === "workshop") sourceList = workshopData.trainableTroops || []
                                
                                var relativeId = globalToRelativeId(troopId)
                                
                                for (var i = 0; i < sourceList.length; i++) {
                                    // Backend parser her zaman relative ID (t1, t2...) döndürür
                                    // Bizim elimizdeki troopId global (u11, u21...) olabilir
                                    // Bu yüzden convert edip karşılaştırıyoruz
                                    var tid = sourceList[i].troopId 
                                    if (tid === relativeId) {
                                        return true
                                    }
                                    if (tid === troopId) return true
                                }
                                return false
                            }

                            function hasBuilding(buildingType) {
                                var gidMap = {"barracks": "19", "stable": "20", "workshop": "21"}
                                var gid = gidMap[buildingType]
                                if (!gid) return false
                                for (var i = 0; i < allBuildings.length; i++) {
                                    if (String(allBuildings[i].gid) === gid) return true
                                }
                                return false
                            }

                            function isAutoTrainTroop(troopId) {
                                if (!modelObj) return false
                                var configs = modelObj.troopConfigs
                                var vid = selectedVillage()
                                for (var i = 0; i < configs.length; i++) {
                                    if (configs[i].villageId === vid && configs[i].troopId === troopId) {
                                        return true
                                    }
                                }
                                return false
                            }

                            // Koydeki Askerler
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 6
                                Layout.topMargin: 12
                                implicitHeight: troopsContent.implicitHeight + 24
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    id: troopsContent
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        text: "Koydeki Askerler"
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    Label {
                                        visible: militaryCol.troops.length === 0
                                        text: "Bu koyde asker bulunmuyor."
                                        color: "#666"
                                    }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 6

                                        Repeater {
                                            model: militaryCol.troops

                                            Rectangle {
                                                width: Math.min(150, (militaryCol.width - 40) / 3)
                                                height: 32
                                                radius: 6
                                                color: "#2a3142"

                                                Row {
                                                    anchors.centerIn: parent
                                                    spacing: 6
                                                    Label { text: modelData.count || "0"; color: "#4CAF50"; font.bold: true; font.pixelSize: 12 }
                                                    Label { text: modelData.displayName || "?"; color: "white"; font.pixelSize: 11 }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Roma Asker Tipleri
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 6
                                Layout.topMargin: 0
                                implicitHeight: tribalTroopsContent.implicitHeight + 24
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    id: tribalTroopsContent
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        text: tribeToName(militaryCol.tribe) + " Asker Tipleri"
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    GridLayout {
                                        Layout.fillWidth: true
                                        columns: Math.max(1, Math.floor((parent.width) / 190))
                                        rowSpacing: 6
                                        columnSpacing: 8

                                        Repeater {
                                            model: militaryCol.tribeTroops

                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.minimumWidth: 160
                                                height: militaryCol.isTroopUnlocked(modelData.id) ? 70 : 44
                                                radius: 6
                                                color: militaryCol.isAutoTrainTroop(modelData.id) ? "#1B3A2A" : (militaryCol.isTroopUnlocked(modelData.id) ? "#2a3142" : "#1a1f28")
                                                border.width: militaryCol.isAutoTrainTroop(modelData.id) ? 2 : (militaryCol.getTroopCount(modelData.id) > 0 ? 1 : 0)
                                                border.color: militaryCol.isAutoTrainTroop(modelData.id) ? "#4CAF50" : "#4CAF50"

                                                ColumnLayout {
                                                    anchors.fill: parent
                                                    anchors.margins: 8
                                                    spacing: 4

                                                    RowLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 6

                                                        Rectangle {
                                                            width: 28
                                                            height: 28
                                                            radius: 4
                                                            color: militaryCol.isTroopUnlocked(modelData.id) ? "#3a7bd5" : "#444"

                                                            Label {
                                                                anchors.centerIn: parent
                                                                text: militaryCol.getTroopCount(modelData.id)
                                                                color: "white"
                                                                font.pixelSize: 11
                                                                font.bold: true
                                                            }
                                                        }

                                                        ColumnLayout {
                                                            Layout.fillWidth: true
                                                            spacing: 0

                                                            Label {
                                                                text: modelData.name
                                                                color: militaryCol.isTroopUnlocked(modelData.id) ? "white" : "#666"
                                                                font.pixelSize: 11
                                                                elide: Text.ElideRight
                                                                Layout.fillWidth: true
                                                            }

                                                            Label {
                                                                text: militaryCol.isTroopUnlocked(modelData.id) ? "Acik" : (militaryCol.hasBuilding(modelData.building) ? "Kitli" : "Bina yok")
                                                                color: militaryCol.isTroopUnlocked(modelData.id) ? "#4CAF50" : "#F44336"
                                                                font.pixelSize: 9
                                                            }
                                                        }
                                                    }

                                                    // Auto-train button (only for unlocked troops)
                                                    Rectangle {
                                                        visible: militaryCol.isTroopUnlocked(modelData.id)
                                                        Layout.fillWidth: true
                                                        height: 22
                                                        radius: 4
                                                        color: {
                                                            if (militaryCol.isAutoTrainTroop(modelData.id)) {
                                                                return autoTrainRemoveMa.containsMouse ? "#C62828" : "#E53935"
                                                            }
                                                            return autoTrainMa.containsMouse ? "#2E7D32" : "#388E3C"
                                                        }

                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: militaryCol.isAutoTrainTroop(modelData.id) ? "Otomatik: AKTIF" : "Otomatik Bas"
                                                            color: "white"
                                                            font.pixelSize: 9
                                                            font.bold: true
                                                        }

                                                        MouseArea {
                                                            id: autoTrainMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            visible: !militaryCol.isAutoTrainTroop(modelData.id)
                                                            onClicked: {
                                                                if (modelObj) {
                                                                    // Default 5 dakika aralık
                                                                    modelObj.setVillageTroop(selectedVillage(), modelData.id, modelData.name, modelData.building, 5)
                                                                }
                                                            }
                                                        }

                                                        MouseArea {
                                                            id: autoTrainRemoveMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            visible: militaryCol.isAutoTrainTroop(modelData.id)
                                                            onClicked: {
                                                                if (modelObj) {
                                                                    modelObj.removeVillageTroop(selectedVillage(), modelData.building)
                                                                }
                                                            }
                                                        }
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Askeri Binalar
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 6
                                Layout.topMargin: 0
                                implicitHeight: milBuildingsContent.implicitHeight + 24
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    id: milBuildingsContent
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        text: "Askeri Binalar"
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    Label {
                                        visible: militaryCol.getMilitaryBuildings().length === 0
                                        text: "Bu koyde askeri bina yok."
                                        color: "#666"
                                    }

                                    Flow {
                                        Layout.fillWidth: true
                                        spacing: 8

                                        Repeater {
                                            model: militaryCol.getMilitaryBuildings()

                                            Rectangle {
                                                width: Math.min(120, (militaryCol.width - 40) / 3)
                                                height: 36
                                                radius: 6
                                                color: "#2a3142"

                                                Row {
                                                    anchors.centerIn: parent
                                                    spacing: 6

                                                    Rectangle {
                                                        width: 24
                                                        height: 24
                                                        radius: 4
                                                        color: "#3a7bd5"
                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: modelData.gid === "19" ? "K" : (modelData.gid === "20" ? "A" : "T")
                                                            color: "white"
                                                            font.pixelSize: 12
                                                            font.bold: true
                                                        }
                                                    }

                                                    Label {
                                                        anchors.verticalCenter: parent.verticalCenter
                                                        text: gidToName(modelData.gid)
                                                        color: "white"
                                                        font.pixelSize: 11
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Otomatik Asker Basma Paneli
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 6
                                Layout.topMargin: 0
                                implicitHeight: autoTrainPanelContent.implicitHeight + 24
                                radius: 10
                                color: "#1f2430"

                                ColumnLayout {
                                    id: autoTrainPanelContent
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 8

                                    Label {
                                        text: "Otomatik Asker Basma"
                                        color: "white"
                                        font.pixelSize: 15
                                        font.bold: true
                                    }

                                    // Helper function to find config
                                    function findTroopConfig(villageId, building) {
                                        var configs = modelObj ? modelObj.troopConfigs : []
                                        for (var i = 0; i < configs.length; i++) {
                                            if (configs[i].villageId === villageId && configs[i].building === building) {
                                                return configs[i]
                                            }
                                        }
                                        return null
                                    }

                                    // Counter to force timer text re-evaluation
                                    property int troopTickCounter: 0
                                    Connections {
                                        target: modelObj
                                        function onTroopTimerTick() { autoTrainPanelContent.troopTickCounter++ }
                                        function onTroopConfigsChanged() { autoTrainPanelContent.troopTickCounter++ }
                                    }

                                    // Configured troops for current village
                                    property var currentVillageTroops: {
                                        var tick = autoTrainPanelContent.troopTickCounter
                                        var vid = selectedVillage()
                                        var configs = modelObj ? modelObj.troopConfigs : []
                                        var result = []
                                        for (var i = 0; i < configs.length; i++) {
                                            if (configs[i].villageId === vid) {
                                                result.push(configs[i])
                                            }
                                        }
                                        return result
                                    }

                                    Label {
                                        visible: autoTrainPanelContent.currentVillageTroops.length === 0
                                        text: "Bu köy için otomatik asker basma ayarı yok.\nAsker listesinden otomatik basma ekleyebilirsiniz."
                                        color: "#888"
                                        font.pixelSize: 12
                                    }

                                    // List of configured troops
                                    Repeater {
                                        model: autoTrainPanelContent.currentVillageTroops

                                        Rectangle {
                                            Layout.fillWidth: true
                                            height: troopItemCol.implicitHeight + 20
                                            radius: 8
                                            property var troopCfg: modelData
                                            property bool isEnabled: troopCfg ? troopCfg.enabled : false
                                            color: isEnabled ? "#1B3A2A" : "#1e2433"
                                            border.color: isEnabled ? "#4CAF50" : "#2a3142"
                                            border.width: 1

                                            ColumnLayout {
                                                id: troopItemCol
                                                anchors.left: parent.left
                                                anchors.right: parent.right
                                                anchors.top: parent.top
                                                anchors.margins: 10
                                                spacing: 8

                                                RowLayout {
                                                    Layout.fillWidth: true
                                                    spacing: 10

                                                    ColumnLayout {
                                                        Layout.fillWidth: true
                                                        spacing: 2

                                                        Label {
                                                            text: modelData.troopName || "Asker"
                                                            color: "white"
                                                            font.pixelSize: 14
                                                            font.bold: true
                                                        }

                                                        Label {
                                                            text: {
                                                                var buildingName = modelData.building === "barracks" ? "Kışla" :
                                                                                 (modelData.building === "stable" ? "Ahır" : "Atölye")
                                                                return buildingName + " | " + modelData.intervalMinutes + " dakika aralık"
                                                            }
                                                            color: "#888"
                                                            font.pixelSize: 11
                                                        }
                                                    }

                                                    // Enable/Disable toggle
                                                    Rectangle {
                                                        width: 80
                                                        height: 30
                                                        radius: 6
                                                        color: toggleMa.pressed ? (modelData.enabled ? "#2a5230" : "#5a3030") :
                                                               (toggleMa.containsMouse ? (modelData.enabled ? "#3a6340" : "#6a4040") :
                                                               (modelData.enabled ? "#4CAF50" : "#7a5050"))

                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: modelData.enabled ? "Aktif" : "Pasif"
                                                            color: "white"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }

                                                        MouseArea {
                                                            id: toggleMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (modelObj) {
                                                                    modelObj.setVillageTroopEnabled(
                                                                        selectedVillage(),
                                                                        modelData.building,
                                                                        !modelData.enabled
                                                                    )
                                                                }
                                                            }
                                                        }
                                                    }

                                                    // Remove button
                                                    Rectangle {
                                                        width: 80
                                                        height: 30
                                                        radius: 6
                                                        color: removeMa.pressed ? "#8a3030" : (removeMa.containsMouse ? "#9a4040" : "#aa5050")

                                                        Label {
                                                            anchors.centerIn: parent
                                                            text: "Kaldır"
                                                            color: "white"
                                                            font.pixelSize: 11
                                                            font.bold: true
                                                        }

                                                        MouseArea {
                                                            id: removeMa
                                                            anchors.fill: parent
                                                            hoverEnabled: true
                                                            cursorShape: Qt.PointingHandCursor
                                                            onClicked: {
                                                                if (modelObj) {
                                                                    modelObj.removeVillageTroop(selectedVillage(), modelData.building)
                                                                }
                                                            }
                                                        }
                                                    }
                                                }

                                                // Timer display (when enabled)
                                                Label {
                                                    visible: modelData.enabled
                                                    text: {
                                                        var tick = autoTrainPanelContent.troopTickCounter
                                                        var remaining = modelData.remainingSeconds || 0
                                                        if (remaining <= 0) return "Şimdi basılacak..."
                                                        var m = Math.floor(remaining / 60)
                                                        var s = remaining % 60
                                                        return "Sonraki basım: " + m + "dk " + s + "sn"
                                                    }
                                                    color: "#66bb6a"
                                                    font.pixelSize: 11
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // Debug bilgisi
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.margins: 6
                                Layout.topMargin: 0
                                height: 60
                                radius: 10
                                color: "#151a22"

                                ColumnLayout {
                                    anchors.fill: parent
                                    anchors.margins: 12
                                    spacing: 4

                                    Label {
                                        text: "Debug: Barracks veri durumu"
                                        color: "#666"
                                        font.pixelSize: 11
                                    }
                                    Label {
                                        text: "trainableTroops: " + militaryCol.trainableTroops.length + " | lockedTroops: " + militaryCol.lockedTroops.length
                                        color: "#888"
                                        font.pixelSize: 10
                                    }
                                    Label {
                                        text: "barracksData keys: " + Object.keys(militaryCol.barracksData).join(", ")
                                        color: "#888"
                                        font.pixelSize: 10
                                    }
                                }
                            }
                        }
                    }

                    // === TAB 4: Yağma Listeleri ===
                    Flickable {
                        clip: true
                        contentHeight: farmCol.implicitHeight + 40

                        ColumnLayout {
                            id: farmCol
                            width: parent.width - 24
                            anchors.horizontalCenter: parent.horizontalCenter
                            spacing: 12

                            // Fetch farm lists button
                            Rectangle {
                                Layout.fillWidth: true
                                height: 44
                                radius: 8
                                color: fetchFarmMa.pressed ? "#2a5db0" : (fetchFarmMa.containsMouse ? "#3a6dc0" : "#3a7bd5")

                                Label {
                                    anchors.centerIn: parent
                                    text: "Yagma Listelerini Getir"
                                    color: "white"
                                    font.pixelSize: 14
                                    font.bold: true
                                }

                                MouseArea {
                                    id: fetchFarmMa
                                    anchors.fill: parent
                                    hoverEnabled: true
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        if (modelObj) modelObj.fetchFarmLists(selectedVillage())
                                    }
                                }
                            }

                            // Helper function to find config for a list
                            function findFarmConfig(listId) {
                                var configs = modelObj ? modelObj.farmConfigs : []
                                for (var i = 0; i < configs.length; i++) {
                                    if (configs[i].listId === listId) return configs[i]
                                }
                                return null
                            }

                            // Counter to force timer text re-evaluation
                            property int farmTickCounter: 0
                            Connections {
                                target: modelObj
                                function onFarmTimerTick() { farmCol.farmTickCounter++ }
                                function onFarmConfigsChanged() { farmCol.farmTickCounter++ }
                            }

                            // Köy bazlı filtrelenmiş farm listeleri
                            property var filteredFarmLists: {
                                // availableFarmLists değiştiğinde yeniden hesapla
                                var allLists = modelObj ? modelObj.availableFarmLists : []
                                var vid = selectedVillage()
                                if (!modelObj) return []
                                return modelObj.farmListsForVillage(vid)
                            }

                            // Available Farm Lists - each with independent config
                            Repeater {
                                model: farmCol.filteredFarmLists

                                Rectangle {
                                    Layout.fillWidth: true
                                    height: farmItemCol.implicitHeight + 20
                                    radius: 8
                                    property var farmCfg: {
                                        // farmTickCounter forces re-evaluation on config changes
                                        var tick = farmCol.farmTickCounter
                                        return farmCol.findFarmConfig(modelData.id)
                                    }
                                    property bool isConfigured: farmCfg !== null
                                    property bool isEnabled: farmCfg ? farmCfg.enabled : false
                                    color: isEnabled ? "#1B3A2A" : (isConfigured ? "#1e2838" : "#1e2433")
                                    border.color: isEnabled ? "#4CAF50" : (isConfigured ? "#3a6dc0" : "#2a3142")
                                    border.width: 1

                                    ColumnLayout {
                                        id: farmItemCol
                                        anchors.left: parent.left
                                        anchors.right: parent.right
                                        anchors.top: parent.top
                                        anchors.margins: 10
                                        spacing: 8

                                        // Header row: name + slots + send now button
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 10

                                            ColumnLayout {
                                                Layout.fillWidth: true
                                                spacing: 2

                                                Label {
                                                    text: modelData.name || ("Liste " + modelData.id)
                                                    color: "white"
                                                    font.pixelSize: 14
                                                    font.bold: true
                                                }

                                                Label {
                                                    text: {
                                                        var info = "ID: " + modelData.id
                                                        if (modelData.slotsAmount !== undefined) {
                                                            info += " | " + modelData.slotsAmount + " hedef"
                                                        }
                                                        return info
                                                    }
                                                    color: "#888"
                                                    font.pixelSize: 11
                                                }
                                            }

                                            // Send now button
                                            Rectangle {
                                                width: 100
                                                height: 30
                                                radius: 6
                                                color: farmSendMa.pressed ? "#D32F2F" : (farmSendMa.containsMouse ? "#E53935" : "#FF5722")

                                                Label {
                                                    anchors.centerIn: parent
                                                    text: "Gonder"
                                                    color: "white"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }

                                                MouseArea {
                                                    id: farmSendMa
                                                    anchors.fill: parent
                                                    hoverEnabled: true
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (modelObj) modelObj.executeFarmListNow(modelData.id)
                                                    }
                                                }
                                            }
                                        }

                                        // Config row: interval + start/stop + timer
                                        RowLayout {
                                            Layout.fillWidth: true
                                            spacing: 8

                                            Label {
                                                text: "Her"
                                                color: "#ccc"
                                                font.pixelSize: 12
                                            }

                                            SpinBox {
                                                id: farmListInterval
                                                from: 5
                                                to: 180
                                                value: farmCfg ? farmCfg.intervalMinutes : 30
                                                implicitWidth: 100
                                                onValueModified: {
                                                    if (!modelObj) return
                                                    var vid = selectedVillage()
                                                    var name = modelData.name || ("Liste " + modelData.id)
                                                    modelObj.setFarmListConfig(modelData.id, vid, name, value)
                                                }
                                            }

                                            Label {
                                                text: "dk"
                                                color: "#ccc"
                                                font.pixelSize: 12
                                            }

                                            // Start/Stop button
                                            Rectangle {
                                                width: 80
                                                height: 30
                                                radius: 6
                                                color: isEnabled ? "#F44336" : "#4CAF50"

                                                Label {
                                                    anchors.centerIn: parent
                                                    text: isEnabled ? "Durdur" : "Baslat"
                                                    color: "white"
                                                    font.pixelSize: 12
                                                    font.bold: true
                                                }

                                                MouseArea {
                                                    anchors.fill: parent
                                                    cursorShape: Qt.PointingHandCursor
                                                    onClicked: {
                                                        if (!modelObj) return
                                                        var vid = selectedVillage()
                                                        var name = modelData.name || ("Liste " + modelData.id)
                                                        // If not configured yet, create config first
                                                        if (!isConfigured) {
                                                            modelObj.setFarmListConfig(modelData.id, vid, name, farmListInterval.value)
                                                        }
                                                        modelObj.setFarmListEnabled(modelData.id, !isEnabled)
                                                    }
                                                }
                                            }

                                            // Timer countdown display
                                            Label {
                                                visible: isEnabled
                                                text: {
                                                    // farmTickCounter forces re-evaluation
                                                    var tick = farmCol.farmTickCounter
                                                    if (!modelObj) return ""
                                                    var secs = modelObj.farmRemainingSeconds(modelData.id)
                                                    if (secs <= 0) return ""
                                                    var m = Math.floor(secs / 60)
                                                    var s = secs % 60
                                                    return m + ":" + (s < 10 ? "0" : "") + s
                                                }
                                                color: "#4CAF50"
                                                font.pixelSize: 13
                                                font.bold: true
                                            }
                                        }

                                        // Remove config button (only if configured)
                                        Rectangle {
                                            visible: isConfigured
                                            Layout.alignment: Qt.AlignRight
                                            width: 90
                                            height: 24
                                            radius: 4
                                            color: farmRemMa.containsMouse ? "#555" : "transparent"
                                            border.color: "#555"
                                            border.width: 1

                                            Label {
                                                anchors.centerIn: parent
                                                text: "Ayari Sil"
                                                color: "#aaa"
                                                font.pixelSize: 10
                                            }

                                            MouseArea {
                                                id: farmRemMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (modelObj) modelObj.removeFarmListConfig(modelData.id)
                                                }
                                            }
                                        }
                                    }
                                }
                            }

                            // No lists message
                            Label {
                                visible: !farmCol.filteredFarmLists || farmCol.filteredFarmLists.length === 0
                                text: "Bu koy icin yagma listesi bulunamadi.\nListeler otomatik yuklenecek veya\n'Yagma Listelerini Getir' butonuna tiklayin."
                                color: "#888"
                                font.pixelSize: 13
                                horizontalAlignment: Text.AlignHCenter
                                Layout.fillWidth: true
                                Layout.topMargin: 20
                            }
                        }
                    }

                }
            }
        }

        // Sağ %30: Activity Log
        Rectangle {
            Layout.preferredWidth: win.width * 0.3
            Layout.fillHeight: true
            radius: 12
            color: "#1f2430"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 12
                spacing: 10

                Label {
                    text: "📋 Aktivite Logu"
                    color: "white"
                    font.pixelSize: 16
                    font.bold: true
                }

                ListView {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    model: modelObj ? modelObj.activityLog : []
                    clip: true
                    spacing: 6

                    delegate: Rectangle {
                        width: ListView.view.width
                        height: contentCol.implicitHeight + 20
                        radius: 6
                        color: {
                            var t = modelData.type || "info"
                            if (t === "error") return "#5D2A2A"
                            if (t === "warning") return "#5D4A2A"
                            if (t === "success") return "#2A5D3A"
                            return "#2a3142"
                        }

                        ColumnLayout {
                            id: contentCol
                            anchors.fill: parent
                            anchors.margins: 10
                            spacing: 4

                            Label {
                                text: modelData.timestamp || ""
                                color: "#aaa"
                                font.pixelSize: 9
                                font.family: "Menlo"
                            }

                            Label {
                                text: modelData.message || ""
                                color: "white"
                                font.pixelSize: 11
                                wrapMode: Text.WordWrap
                                Layout.fillWidth: true
                            }
                        }
                    }
                }
            }
        }
    }

    // Build Queue Panel (Sliding from bottom)
    Rectangle {
        id: queuePanel
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: queuePanelOpen ? 200 : 40
        color: "#1a1f2e"
        border.color: "#2a3142"
        border.width: 1
        z: 100

        property bool queuePanelOpen: false
        property var queue: modelObj ? modelObj.buildQueue : []

        // Group tasks by villageId
        function groupedQueue() {
            var groups = {}
            var order = []
            for (var i = 0; i < queue.length; i++) {
                var vid = queue[i].villageId
                if (!(vid in groups)) {
                    groups[vid] = []
                    order.push(vid)
                }
                groups[vid].push(queue[i])
            }
            var result = []
            for (var j = 0; j < order.length; j++) {
                result.push({ villageId: order[j], tasks: groups[order[j]] })
            }
            return result
        }

        function villageNameById(vid) {
            if (!villages) return "Köy " + vid
            for (var i = 0; i < villages.length; i++) {
                if (villages[i].id === vid) return villages[i].name || ("Köy " + vid)
            }
            return "Köy " + vid
        }

        Behavior on height {
            NumberAnimation { duration: 200; easing.type: Easing.OutCubic }
        }

        // Header with drag handle
        Rectangle {
            id: queueHeader
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 40
            color: "#252b3d"
            
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: queuePanel.queuePanelOpen = !queuePanel.queuePanelOpen
            }

            RowLayout {
                anchors.fill: parent
                anchors.margins: 10
                spacing: 10

                Label {
                    text: queuePanel.queuePanelOpen ? "▼" : "▲"
                    color: "#888"
                    font.pixelSize: 14
                }

                Label {
                    text: "İnşaat Kuyruğu (" + queuePanel.queue.length + " görev)"
                    color: "white"
                    font.bold: true
                    font.pixelSize: 14
                }

                Rectangle {
                    Layout.fillWidth: true
                    height: 1
                    color: "transparent"
                }

                Label {
                    text: queuePanel.queuePanelOpen ? "Kapat" : "Aç"
                    color: "#4CAF50"
                    font.pixelSize: 12
                }
            }
        }

        // Queue content - grouped by village
        Flickable {
            id: queueFlickable
            anchors.top: queueHeader.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.bottom: parent.bottom
            anchors.margins: 10
            clip: true
            visible: queuePanel.queuePanelOpen
            contentWidth: queueCol.implicitWidth
            contentHeight: queueCol.implicitHeight

            Column {
                id: queueCol
                width: queueFlickable.width
                spacing: 12

                Repeater {
                    model: queuePanel.groupedQueue()

                    Column {
                        width: parent.width
                        spacing: 6

                        // Village header
                        Label {
                            text: queuePanel.villageNameById(modelData.villageId) + " (" + modelData.tasks.length + " görev)"
                            color: "#81C784"
                            font.bold: true
                            font.pixelSize: 13
                        }

                        Flow {
                            width: parent.width
                            spacing: 8

                            Repeater {
                                model: modelData.tasks

                                Rectangle {
                                    width: 260
                                    height: 60
                                    radius: 6
                                    color: "#2a3142"

                                    RowLayout {
                                        anchors.fill: parent
                                        anchors.margins: 8
                                        spacing: 8

                                        Column {
                                            Layout.fillWidth: true
                                            spacing: 2

                                            Label {
                                                text: "#" + (index + 1) + " " + (modelData.buildingName || "?")
                                                color: "white"
                                                font.pixelSize: 12
                                                font.bold: true
                                                elide: Text.ElideRight
                                                width: parent.width
                                            }

                                            Label {
                                                text: "Seviye " + (modelData.currentLevel || "?") + " → " + modelData.targetLevel
                                                color: "#4CAF50"
                                                font.pixelSize: 11
                                            }
                                        }

                                        // Delete button
                                        Rectangle {
                                            width: 28
                                            height: 28
                                            radius: 4
                                            color: queueDelMa.pressed ? "#C62828" : (queueDelMa.containsMouse ? "#D32F2F" : "#F44336")

                                            Label {
                                                anchors.centerIn: parent
                                                text: "X"
                                                color: "white"
                                                font.pixelSize: 12
                                                font.bold: true
                                            }

                                            MouseArea {
                                                id: queueDelMa
                                                anchors.fill: parent
                                                hoverEnabled: true
                                                cursorShape: Qt.PointingHandCursor
                                                onClicked: {
                                                    if (modelObj) {
                                                        modelObj.removeFromBuildQueue(modelData.villageId, index)
                                                    }
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            Label {
                anchors.centerIn: parent
                visible: queuePanel.queue.length === 0
                text: "Kuyruk boş\n\nBina kartlarından 'Kuyruğa' butonunu kullanarak görev ekleyin"
                color: "#666"
                font.pixelSize: 12
                horizontalAlignment: Text.AlignHCenter
            }
        }
    }
    } // Item wrapper

    // Telegram Info Dialog
    Dialog {
        id: telegramInfoDialog
        title: "Telegram Bildirim Kurulumu"
        x: (parent.width - width) / 2
        y: (parent.height - height) / 2
        width: 450
        standardButtons: Dialog.Ok
        modal: true
        
        background: Rectangle {
            color: "#1f2430"
            border.color: "#3a7bd5"
            radius: 8
        }
        
        contentItem: ColumnLayout {
            spacing: 12
            width: parent.width

            Label {
                text: "Telegram bildirimlerini almak için:"
                font.bold: true
                font.pixelSize: 16
                color: "white"
            }

            Label {
                text: "1. Telegram'da <b>@travian_saldiri_uyari_bot</b>'u bulun."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                textFormat: Text.RichText
                color: "#ccc"
                font.pixelSize: 13
            }

            Label {
                text: "2. Bot ile konuşmayı başlatın (Start'a basın)."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#ccc"
                font.pixelSize: 13
            }

            Label {
                text: "3. Bot size Chat ID'nizi söyleyebilir veya başka bir bot (örn: @usegetid_bot) ile ID'nizi öğrenebilirsiniz."
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                color: "#ccc"
                font.pixelSize: 13
            }

            Label {
                text: "4. <b>config/settings.ini</b> dosyasını açın ve şu ayarı yapın:"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                textFormat: Text.RichText
                color: "#ccc"
                font.pixelSize: 13
            }

            TextArea {
                text: "[Telegram]\nchatId=SENIN_CHAT_IDN"
                readOnly: true
                Layout.fillWidth: true
                color: "#aaffaa"
                font.family: "Courier"
                background: Rectangle { color: "#111"; radius: 4; border.color: "#555" }
            }
            
            Label {
                text: "Not: Chat ID girilmezse bildirim çalışmaz."
                font.italic: true
                color: "#ff5555"
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
                font.pixelSize: 12
            }
        }
    }
}
}
