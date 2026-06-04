import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: libraryRoot
    signal openDocument(string path, int lastPage, real zoom, int rotation)
    signal openSettings()

    property string currentView: "all" // all | favorites
    property string viewMode: "grid" // grid | list
    property string searchQuery: ""
    property int selectedFolderIndex: -1 // -1 => all files
    property int folderCounter: 1
    property var folderDocs: ({}) // { folderName: [filePath, ...] }

    property string moveTargetPath: ""
    property int moveTargetFolderIndex: -1

    ListModel { id: foldersModel }

    // Unified application theme manager
    Theme {
        id: appTheme
    }

    Shortcut {
        sequence: "Ctrl+F"
        onActivated: {
            // Focus Search Bar area
        }
    }
    Shortcut {
        sequence: "Esc"
        onActivated: {
            searchQuery = ""
        }
    }
    Shortcut {
        sequence: "Ctrl+O"
        onActivated: libraryModel.requestUpload()
    }
    Shortcut {
        sequence: "Ctrl+N"
        onActivated: createFolder()
    }
    Shortcut {
        sequence: "Ctrl+,"
        onActivated: libraryRoot.openSettings()
    }

    function folderNameAt(idx) {
        if (idx < 0 || idx >= foldersModel.count) return ""
        return foldersModel.get(idx).name
    }

    function createFolder() {
        const name = "Folder " + folderCounter
        foldersModel.append({ name: name })
        folderDocs[name] = []
        folderDocs = Object.assign({}, folderDocs)
        selectedFolderIndex = foldersModel.count - 1
        folderCounter += 1
    }

    function deleteSelectedFolder() {
        if (selectedFolderIndex < 0 || selectedFolderIndex >= foldersModel.count) return
        const name = folderNameAt(selectedFolderIndex)
        delete folderDocs[name]
        folderDocs = Object.assign({}, folderDocs)
        foldersModel.remove(selectedFolderIndex)
        selectedFolderIndex = -1
    }

    function matchesSearch(title) {
        if (!searchQuery || searchQuery.length === 0) return true
        return title.toLowerCase().indexOf(searchQuery.toLowerCase()) !== -1
    }

    function isDocInSelectedFolder(filePath) {
        if (selectedFolderIndex < 0) return true
        const name = folderNameAt(selectedFolderIndex)
        const arr = folderDocs[name] || []
        return arr.indexOf(filePath) !== -1
    }

    function addDocToFolderByName(filePath, folderName) {
        if (!folderName || folderName.length === 0) return
        const arr = (folderDocs[folderName] || []).slice()
        if (arr.indexOf(filePath) === -1) {
            arr.push(filePath)
            folderDocs[folderName] = arr
            folderDocs = Object.assign({}, folderDocs)
        }
    }

    function removeDocFromFolderByName(filePath, folderName) {
        if (!folderName || folderName.length === 0) return
        const arr = (folderDocs[folderName] || []).slice()
        const i = arr.indexOf(filePath)
        if (i !== -1) {
            arr.splice(i, 1)
            folderDocs[folderName] = arr
            folderDocs = Object.assign({}, folderDocs)
        }
    }

    // Main Workspace Background fill
    Rectangle {
        anchors.fill: parent
        color: appTheme.colors.bg
    }

    Dialog {
        id: moveDialog
        modal: true
        width: 360
        x: (libraryRoot.width - width) / 2
        y: (libraryRoot.height - height) / 2
        title: "Move to Folder"
        standardButtons: Dialog.Cancel
        
        background: Rectangle {
            color: "#ffffff"
            radius: 12
            border.color: appTheme.colors.line
            border.width: 1
        }
        
        header: Label {
            text: "Move to Folder"
            font.family: appTheme.typography.family
            font.pixelSize: 16
            font.weight: Font.Bold
            color: appTheme.colors.textMain
            padding: 16
        }

        contentItem: ColumnLayout {
            spacing: 16
            Label { 
                text: "Select container folder for this document:"
                font.family: appTheme.typography.family
                font.pixelSize: 13
                color: appTheme.colors.textSub 
            }
            ComboBox {
                id: folderCombo
                Layout.fillWidth: true
                model: foldersModel
                textRole: "name"
                currentIndex: moveTargetFolderIndex >= 0 ? moveTargetFolderIndex : 0
                
                delegate: ItemDelegate {
                    width: folderCombo.width
                    contentItem: Text {
                        text: model.name
                        color: appTheme.colors.textMain
                        font.family: appTheme.typography.family
                        font.pixelSize: 13
                        elide: Text.ElideRight
                        verticalAlignment: Text.AlignVCenter
                    }
                    highlighted: folderCombo.highlightedIndex === index
                }
            }
            Button {
                id: confirmMoveBtn
                text: "Move"
                Layout.fillWidth: true
                Layout.preferredHeight: 40
                enabled: foldersModel.count > 0 && moveTargetPath.length > 0
                hoverEnabled: true

                background: Rectangle {
                    color: confirmMoveBtn.enabled 
                        ? (confirmMoveBtn.pressed ? appTheme.colors.primaryHover : (confirmMoveBtn.hovered ? appTheme.colors.primaryHover : appTheme.colors.primary))
                        : appTheme.colors.line
                    radius: 8
                    Behavior on color { ColorAnimation { duration: 150 } }
                }

                contentItem: Text {
                    text: confirmMoveBtn.text
                    font.family: appTheme.typography.family
                    font.pixelSize: 13
                    font.weight: Font.DemiBold
                    color: confirmMoveBtn.enabled ? "#ffffff" : appTheme.colors.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                }
                
                onClicked: {
                    if (folderCombo.currentIndex >= 0 && folderCombo.currentIndex < foldersModel.count) {
                        libraryRoot.addDocToFolderByName(moveTargetPath, foldersModel.get(folderCombo.currentIndex).name)
                    }
                    moveDialog.close()
                }
            }
        }
    }

    RowLayout {
        anchors.fill: parent
        spacing: 0

        // Use new custom high-polish sidebar component
        Sidebar {
            id: mainSidebar
            foldersModel: foldersModel
            selectedFolderIndex: libraryRoot.selectedFolderIndex
            folderDocs: libraryRoot.folderDocs
            currentView: libraryRoot.currentView

            onRequestUpload: libraryModel.requestUpload()
            onOpenSettings: libraryRoot.openSettings()
            onCreateFolder: libraryRoot.createFolder()
            onDeleteFolder: libraryRoot.deleteSelectedFolder()
            onSelectFolder: (index) => {
                libraryRoot.selectedFolderIndex = index
                libraryRoot.currentView = mainSidebar.currentView
            }
            onCurrentViewChanged: {
                libraryRoot.currentView = mainSidebar.currentView
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 12

            // Expose the custom top bar with sidebar support
            TopBar {
                id: searchAndViewBar
                sidebarCollapsed: mainSidebar.collapsed
                onSearchQueryChanged: {
                    libraryRoot.searchQuery = searchAndViewBar.searchQuery
                }
                onToggleSidebar: {
                    mainSidebar.collapsed = !mainSidebar.collapsed
                }
            }

            // Category title header block
            Item {
                id: headerBlock
                Layout.fillWidth: true
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                Layout.bottomMargin: 4
                Layout.preferredHeight: Math.max(headerTextLayout.implicitHeight, toggleButtonsLayout.implicitHeight)

                ColumnLayout {
                    id: headerTextLayout
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 2
                    
                    Label {
                        text: libraryRoot.selectedFolderIndex < 0 ? "Library" : libraryRoot.folderNameAt(libraryRoot.selectedFolderIndex)
                        font.family: window.handwrittenFontName
                        font.pixelSize: 42
                        font.weight: Font.DemiBold
                        color: appTheme.colors.textMain
                    }
                    Label {
                        text: libraryRoot.selectedFolderIndex < 0 ? "Manage your textbooks and academic research documents." : ("Private collection • " + libraryRoot.folderNameAt(libraryRoot.selectedFolderIndex))
                        color: appTheme.colors.textSub
                        font.family: appTheme.typography.family
                        font.pixelSize: appTheme.typography.body
                    }
                }

                // Grid / List View Indicator Interactive elements on the far right
                RowLayout {
                    id: toggleButtonsLayout
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 8

                    Rectangle {
                        id: gridToggleRec
                        width: 34
                        height: 34
                        radius: 6
                        color: libraryRoot.viewMode === "grid" ? appTheme.colors.primaryLight : "transparent"
                        border.color: libraryRoot.viewMode === "grid" ? appTheme.colors.line : "transparent"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "\ue9b0" // grid icon
                            font.family: window.iconFont
                            font.pixelSize: 16
                            color: libraryRoot.viewMode === "grid" ? appTheme.colors.textMain : appTheme.colors.textSub
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: libraryRoot.viewMode = "grid"
                        }
                    }

                    Rectangle {
                        id: listToggleRec
                        width: 34
                        height: 34
                        radius: 6
                        color: libraryRoot.viewMode === "list" ? appTheme.colors.primaryLight : "transparent"
                        border.color: libraryRoot.viewMode === "list" ? appTheme.colors.line : "transparent"
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "\ue8ef" // list icon
                            font.family: window.iconFont
                            font.pixelSize: 16
                            color: libraryRoot.viewMode === "list" ? appTheme.colors.textMain : appTheme.colors.textSub
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: libraryRoot.viewMode = "list"
                        }
                    }
                }
            }

            // Grid Main Content View
            ScrollView {
                id: docsScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.leftMargin: 12
                Layout.rightMargin: 12
                Layout.bottomMargin: 24
                // Padding is removed from ScrollView so the clipping viewport occupies the entire width.
                // We handle padding via spacers and child margins to prevent hover-scaled cards near edges from being chipped.
                topPadding: 0
                bottomPadding: 0
                leftPadding: 0
                rightPadding: 0
                clip: true

                Column {
                    width: docsScroll.availableWidth
                    spacing: 16

                    Item {
                        width: 1
                        height: 18
                    }

                    Flow {
                        id: docsFlow
                        x: 12
                        width: parent.width - 24
                        spacing: 16
                        property int visibleCount: 0

                        function recalculateVisibleCount() {
                            let count = 0
                            for (let i = 0; i < libraryRepeater.count; ++i) {
                                let item = libraryRepeater.itemAt(i)
                                if (item && item.showCard) {
                                    count++
                                }
                            }
                            visibleCount = count
                        }

                        property int columns: libraryRoot.viewMode === "grid" ? Math.max(2, Math.floor((width + spacing) / (215 + spacing))) : 1
                        property real cardWidth: libraryRoot.viewMode === "grid" ? Math.max(120, Math.floor((width - (columns - 1) * spacing) / columns)) : Math.max(120, width)

                        Repeater {
                            id: libraryRepeater
                            model: libraryModel
                            delegate: Item {
                                id: cardDelegate
                                property bool passView: libraryRoot.currentView === "all" || model.isFavorite
                                property bool passSearch: libraryRoot.matchesSearch(model.title)
                                property bool passFolder: libraryRoot.isDocInSelectedFolder(model.filePath)
                                property bool showCard: passView && passSearch && passFolder

                                width: showCard ? docsFlow.cardWidth : 0
                                height: showCard ? (libraryRoot.viewMode === "grid" ? (docsFlow.cardWidth * 1.414 + 55) : 72) : 0
                                visible: showCard

                                Component.onCompleted: {
                                    Qt.callLater(docsFlow.recalculateVisibleCount)
                                }
                                Component.onDestruction: {
                                    Qt.callLater(docsFlow.recalculateVisibleCount)
                                }
                                onShowCardChanged: {
                                    Qt.callLater(docsFlow.recalculateVisibleCount)
                                }

                                LibraryCard {
                                    anchors.fill: parent
                                    viewMode: libraryRoot.viewMode
                                    docId: model.id
                                    title: model.title
                                    author: model.author
                                    filePath: model.filePath
                                    lastPage: model.lastPage
                                    pageCount: model.pageCount
                                    thumbnailPath: model.thumbnailPath
                                    isFavorite: model.isFavorite
                                    foldersModel: foldersModel
                                    inSelectedFolder: libraryRoot.selectedFolderIndex >= 0 && libraryRoot.isDocInSelectedFolder(model.filePath)

                                    onOpenDocument: {
                                        libraryRoot.openDocument(model.filePath, model.lastPage, model.zoom, model.pageRotation)
                                    }
                                    onToggleFavorite: {
                                        libraryModel.toggleFavorite(model.id)
                                    }
                                    onRemoveDocument: {
                                        libraryModel.removeDocument(model.id)
                                    }
                                    onMoveToFolder: {
                                        libraryRoot.moveTargetPath = model.filePath
                                        libraryRoot.moveTargetFolderIndex = libraryRoot.selectedFolderIndex >= 0 ? libraryRoot.selectedFolderIndex : 0
                                        moveDialog.open()
                                    }
                                    onRemoveFromFolder: {
                                        libraryRoot.removeDocFromFolderByName(model.filePath, libraryRoot.folderNameAt(libraryRoot.selectedFolderIndex))
                                    }
                                }
                            }
                        }
                    }

                    // Dynamic Empty State Dashboard View (placed outside the Flow to break positioning loop)
                    Item {
                        id: emptyStateView
                        width: parent.width
                        height: docsFlow.visibleCount === 0 ? 300 : 0
                        visible: docsFlow.visibleCount === 0

                        ColumnLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "\ue2c8" // Empty collection badge
                                font.family: window.iconFont
                                font.pixelSize: 48
                                color: "#cbd5e1"
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "No documents found"
                                font.family: appTheme.typography.family
                                font.pixelSize: 16
                                font.weight: Font.Medium
                                color: appTheme.colors.textMain
                            }

                            Text {
                                Layout.alignment: Qt.AlignHCenter
                                text: "Add PDF documents using '+ New Entry' to start reading."
                                font.family: appTheme.typography.family
                                font.pixelSize: 12
                                color: appTheme.colors.textSub
                            }
                        }
                    }

                    Item {
                        width: 1
                        height: 18
                    }
                }
            }
        }
    }
}
