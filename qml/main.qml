import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.15

import QtMultimedia
import QtMultimedia 5.15

ApplicationWindow {
    id: mainWindow
    visible: true
    width: 500
    height: 600
    title: "开心消消乐"

    // 开场动画：提供启动接口
    function startIntro() {
        introOverlay.start();
    }
    // 进入地图后的入场动画（步数板与叶子）
    function startEntranceAnimations() {
        // 工具：在运行时创建并启动属性动画
        function animateProperty(targetObj, prop, fromVal, toVal, durationMs) {
            var anim = Qt.createQmlObject(
                'import QtQuick 2.15; NumberAnimation { property: "' + prop + '"; from: ' + fromVal + '; to: ' + toVal + '; duration: ' + durationMs + '; easing.type: Easing.OutCubic }',
                targetObj);
            anim.target = targetObj;
            anim.start();
        }

        // step 自上而下滑入
        step.y = -100; // 初始在上方
        animateProperty(step, "y", step.y, -10, 500);

        // 叶子1-4 从顶部滑入
        leaf1.y = -80; animateProperty(leaf1, "y", leaf1.y, -10, 450);
        leaf2.y = -60; animateProperty(leaf2, "y", leaf2.y, 10, 480);
        leaf3.y = -90; animateProperty(leaf3, "y", leaf3.y, -10, 520);
        leaf4.y = -70; animateProperty(leaf4, "y", leaf4.y, -7, 500);

        // 叶子5 从右侧滑入：动画作用在 offsetX，保持 x 绑定不破坏，宽度变化时仍贴右
        leaf5.offsetX = 200; // 初始右侧偏移，使其在屏幕外
        animateProperty(leaf5, "offsetX", leaf5.offsetX, 0, 500);
    }

    // 开场动画遮罩层（最高层）
    Item {
        id: introOverlay
        anchors.fill: parent
        visible: false
        z: 9999
        property int currentIndex: 0
        property int autoMs: 4000
        // 新增：避免重复连接导致多次递增
        property bool advanceRequested: false

        // 图片资源序列
        property var frames: [
            "qrc:/image/ui/made.png",
            "qrc:/image/ui/backgroundstory1.png",
            "qrc:/image/ui/backgroundstory2.png",
            "qrc:/image/ui/backgroundstory3.jpg"
        ]

        // 背景：纯黑，遮住背后所有界面
        Rectangle {
            anchors.fill: parent
            color: "black"
        }

        // 当前图片
        Image {
            id: introImage
            anchors.fill: parent
            source: introOverlay.frames[introOverlay.currentIndex]
            smooth: true
            fillMode: Image.PreserveAspectFit
            cache: true
            opacity: 0
        }

        // 淡入淡出
        SequentialAnimation {
            id: fadeInAnim
            running: false
            NumberAnimation { target: introImage; property: "opacity"; from: 0; to: 1; duration: 300; easing.type: Easing.OutCubic }
        }
        SequentialAnimation {
            id: fadeOutAnim
            running: false
            NumberAnimation { target: introImage; property: "opacity"; from: 1; to: 0; duration: 250; easing.type: Easing.InCubic }
            // 统一在这里完成切换或结束逻辑，避免重复连接
            onStopped: {
                if (!introOverlay.advanceRequested)
                    return;
                introOverlay.advanceRequested = false;
                if (introOverlay.currentIndex < introOverlay.frames.length - 1) {
                    introOverlay.currentIndex += 1;
                    introImage.source = introOverlay.frames[introOverlay.currentIndex];
                    fadeInAnim.start();
                    autoNextTimer.restart();
                } else {
                    introOverlay.visible = false;
                    mainWindow.startEntranceAnimations();
                }
            }
        }

        // 自动前进计时器
        Timer {
            id: autoNextTimer
            interval: introOverlay.autoMs
            repeat: false
            running: false
            onTriggered: introOverlay.next()
        }

        // 点击跳过或前进
        MouseArea {
            anchors.fill: parent
            onClicked: introOverlay.next()
        }

        // 右上角：跳过按钮（置于最上层）
        Image {
            id: skipBtn
            source: "qrc:/image/ui/skip.png"
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.topMargin: 12
            anchors.rightMargin: 12
            width: 48; height: 48
            fillMode: Image.PreserveAspectFit
            z: 10000
            visible: true
            smooth: true
            // 新增：悬停放大效果
            scale: 1.0
            Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
            MouseArea {
                anchors.fill: parent
                hoverEnabled: true
                onEntered: skipBtn.scale = 1.25
                onExited: skipBtn.scale = 1.0
                onClicked: {
                    // 立即跳过所有开场动画
                    autoNextTimer.stop();
                    introOverlay.advanceRequested = false;
                    introOverlay.visible = false;
                    mainWindow.startEntranceAnimations();
                }
            }
        }

        // 控制逻辑
        function start() {
            currentIndex = 0;
            advanceRequested = false;
            visible = true;
            introImage.opacity = 0;
            introImage.source = frames[currentIndex];
            fadeInAnim.start();
            autoNextTimer.restart();
        }
        function next() {
            autoNextTimer.stop();
            advanceRequested = true;
            fadeOutAnim.start();
        }
    }

    Component.onCompleted: {
        startIntro();
        // 启动背景音乐
        if (bgmPlayer) bgmPlayer.play();
    }

    property int cellSize: Math.min(width, height * 0.7) / 8

    // 使用正确的上下文属性名
    property var gameBoard: gameBoardCpp

    // 音效接口：可覆盖默认映射或统一设置为 comboSfxOverride
    property url comboSfxOverride: ""   // 若设置则统一播放该音效
    property real comboSfxVolume: 0.8

    // 替换单一 MediaPlayer 为播放器池，避免连续播放被打断
    property int comboPlayerIndex: 0

    // 播放器池（4 路轮询），为避免后端不能混音，改为每个播放器独立 AudioOutput
    property real _comboOutVol: comboSfxVolume
    AudioOutput { id: comboOut1; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut2; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut3; volume: mainWindow._comboOutVol }
    AudioOutput { id: comboOut4; volume: mainWindow._comboOutVol }
    MediaPlayer { id: comboPlayer1; audioOutput: comboOut1 }
    MediaPlayer { id: comboPlayer2; audioOutput: comboOut2 }
    MediaPlayer { id: comboPlayer3; audioOutput: comboOut3 }
    MediaPlayer { id: comboPlayer4; audioOutput: comboOut4 }

    function getIdleComboPlayer() {
        // 优先选择空闲播放器；若都在播放则轮询下一个
        var players = [comboPlayer1, comboPlayer2, comboPlayer3, comboPlayer4];
        for (var i = 0; i < players.length; i++) {
            if (players[i].playbackState !== MediaPlayer.PlayingState) return players[i];
        }
        // 都在播放，则取轮询索引
        comboPlayerIndex = (comboPlayerIndex + 1) % players.length;
        return players[comboPlayerIndex];
    }

    function playComboSfx(url) {
        if (!url || String(url).length === 0) return;
        var p = getIdleComboPlayer();
        // 先停止并重置到开头，确保快速连播不受上次播放位置影响
        p.stop();
        p.source = url;
        p.position = 0;
        p.play();
    }

    // 根据连击返回默认音效路径（可按需修改映射）
    function getComboSfx(count) {
        if (mainWindow.comboSfxOverride && String(mainWindow.comboSfxOverride).length > 0) return mainWindow.comboSfxOverride;
        if (count === 2) return "qrc:/music/Good.wav";
        if (count === 3) return "qrc:/music/Great.wav";
        if (count === 4) return "qrc:/music/Excellent.wav";
        if (count === 5) return "qrc:/music/Amazing.wav";
        if (count >= 6) return "qrc:/music/Unbelivable.wav";
        return "";
    }

    // Combo 数字显示区
    Rectangle {
        id: comboDisplay
        width: parent.width
        height: 100
        color: "transparent"  // 保持透明背景

        anchors.top: parent.top
        z: 2  // 通过设置 z 值让它位于其他组件之上

        Text {
            id: comboText
            anchors.centerIn: parent
            font.pixelSize: 50
            color: "white"
            text: "连击 x" + (gameBoard ? gameBoard.comboCount : 0)
            font.bold: true
            opacity: 0 // 初始时隐藏

            // 透明度动画，显示时渐变
            SequentialAnimation {
                id: comboAnimation
                running: gameBoard.comboCount > 1  // 只有连击数大于 1 时，才启动动画
                NumberAnimation {
                    target: comboText
                    property: "opacity"
                    from: 0
                    to: 1
                    duration: 500
                    easing.type: Easing.OutQuad
                }

                SequentialAnimation {
                    ParallelAnimation {
                        NumberAnimation { property: "scale"; to: 1.5; duration: 300; easing.type: Easing.OutElastic }
                        ColorAnimation { target: comboText; property: "color"; to: "yellow"; duration: 300 }
                    }
                    ParallelAnimation {
                        NumberAnimation { property: "scale"; to: 1.0; duration: 300; easing.type: Easing.InElastic }
                        ColorAnimation { target: comboText; property: "color"; to: "white"; duration: 300 }
                    }
                }
            }

            // 定时器，2秒后隐藏文本
            Timer {
                id: hideTimer
                interval: 2000  // 设置为2秒
                running: false
                repeat: false
                onTriggered: {
                    // 使用动画让文本渐变消失
                    var fadeOut = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { target: comboText; property: "opacity"; from: 1; to: 0; duration: 1000; easing.type: Easing.OutQuad }', comboDisplay);
                    fadeOut.start();  // 启动渐变动画
                }
            }

            // 每次动画结束后启动定时器
            onOpacityChanged: {
                if (comboText.opacity === 1) {
                    hideTimer.start()  // 启动定时器
                }
            }
        }

        // 新增：连击图片显示（带闪光背景）
        Item {
            id: comboImageLayer
            anchors.fill: parent
            opacity: 0
            visible: false
            z: 3

            // 背景闪光（放大、右偏、轻微旋转）
            Image {
                id: shiningBg
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: parent.width * 0.3
                width: parent.width * 0.75
                height: parent.height * 1.0
                rotation: 8
                fillMode: Image.PreserveAspectFit
                source: "qrc:/image/ui/Shining.png"
                opacity: 0.85
            }
            // 前景文案图片（放大、右偏、顺时针旋转）
            Image {
                id: comboBadge
                anchors.centerIn: parent
                anchors.horizontalCenterOffset: parent.width * 0.3
                width: parent.width * 0.62
                height: parent.height * 0.82
                rotation: 12
                fillMode: Image.PreserveAspectFit
                // 修复：确保始终返回有效 URL，避免 undefined
                source: {
                    var cc = (gameBoard ? gameBoard.comboCount : 0);
                    if (cc < 2) return "qrc:/image/item/transparent.png"; // 占位透明图
                    if (cc === 2) return "qrc:/image/ui/Good.png";
                    if (cc === 3) return "qrc:/image/ui/Great.png";
                    if (cc === 4) return "qrc:/image/ui/Excellent.png";
                    if (cc === 5) return "qrc:/image/ui/Amazing.png";
                    if (cc >= 6) return "qrc:/image/ui/Unbelievable.png";
                    return "qrc:/image/item/transparent.png";
                }
            }

            // 显示淡入
            SequentialAnimation {
                id: comboImageAnim
                running: false
                NumberAnimation { target: comboImageLayer; property: "opacity"; from: 0; to: 1; duration: 250; easing.type: Easing.OutCubic }
            }

            // 2秒后自动淡出隐藏
            Timer {
                id: comboImageHideTimer
                interval: 2000
                repeat: false
                onTriggered: {
                    var fadeOut = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { target: comboImageLayer; property: "opacity"; from: 1; to: 0; duration: 500; easing.type: Easing.OutQuad }', comboDisplay);
                    fadeOut.onFinished.connect(function(){ comboImageLayer.visible = false; });
                    fadeOut.start();
                }
            }
        }
    }
    Rectangle {
        // 绿叶层：每片叶子独立可调位置与大小
        width: parent.width
        height: 200
        color: "transparent"
        anchors.top: parent.top
        z: 1

        Item {
            id: leavesLayer
            anchors.fill: parent

            // 标题文字
            Item {
                id: title
                x: parent.width / 2 - 100
                y: -15
                width: 200; height: 100
                Image { anchors.fill: parent; source: "qrc:/image/ui/title.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 叶子1
            Item {
                id: leaf1
                x: 0; y: -10
                width: 100; height: 50
                Image { anchors.fill: parent; source: "qrc:/image/ui/leves1.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 叶子2
            Item {
                id: leaf2
                x: 0; y: 10
                width: 100; height: 50
                Image { anchors.fill: parent; source: "qrc:/image/ui/leves2.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 叶子3
            Item {
                id: leaf3
                x: 70; y: -10
                width: 100; height: 50
                Image { anchors.fill: parent; source: "qrc:/image/ui/leves3.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 叶子4
            Item {
                id: leaf4
                x: parent.width - 120; y: -7
                width: 90; height: 50
                Image { anchors.fill: parent; source: "qrc:/image/ui/leves4.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 叶子5
            Item {
                id: leaf5
                // 右侧对齐：x 由父宽度实时计算 + 动画偏移
                property int rightMargin: 75
                property real offsetX: 0
                x: leavesLayer.width - 75 + offsetX
                y: 25
                width: 80; height: 50
                Image { anchors.fill: parent; source: "qrc:/image/ui/leves5.png"; smooth: true; fillMode: Image.PreserveAspectFit }
            }
            // 步数板子
            Item {
                id: step
                x: parent.width - 140; y: -10
                width: 80; height: 80
                Image { anchors.fill: parent; source: "qrc:/image/ui/step.png"; smooth: true; fillMode: Image.PreserveAspectFit }
                Text {
                    anchors.topMargin: 50
                    anchors.centerIn: parent
                    text: "\n步数:\n  " + (gameBoard ? gameBoard.step : 0)
                    font.pixelSize: 16
                    color: "white"
                    font.bold: true
                }
            }
            // 暂停按钮（位于步数板右侧）
            Item {
                id: pauseBtn
                width: 50; height: 50 // 缩小为原来一半
                x: step.x + step.width + 8
                y: step.y + 20 // 微调对齐
                scale: 1.0
                Behavior on scale { NumberAnimation { duration: 120; easing.type: Easing.OutCubic } }
                Image { anchors.fill: parent; source: "qrc:/image/ui/pausebutton.png"; smooth: true; fillMode: Image.PreserveAspectFit }
                MouseArea {
                    anchors.fill: parent
                    hoverEnabled: true
                    onEntered: pauseBtn.scale = 1.25
                    onExited: pauseBtn.scale = 1.0
                    onClicked: pauseOverlay.showPause();
                }
            }
        }
    }

    // 暂停界面覆盖层
    Item {
        id: pauseOverlay
        anchors.fill: parent
        visible: false
        z: 9997
        property bool _pendingRestart: false

        // 新增：对外暴露控制函数，转发到面板
        function showPause() { pausePanel.showPause(); }
        function exitPause() { pausePanel.exitPause(); }

        // 背景半透明灰色（仅点击背景才关闭）
        Rectangle {
            id: pauseBg
            anchors.fill: parent
            color: "#66000000"
            visible: true
            z: 0
        }
        // 新增：拦截底层输入，避免暂停状态还能操作棋盘
        MouseArea {
            id: pauseOverlayBlocker
            anchors.fill: parent
            z: 0.1
            hoverEnabled: true
            acceptedButtons: Qt.AllButtons
            preventStealing: true
            propagateComposedEvents: false
            onPressed: function(){}
            onReleased: function(){}
            onPositionChanged: function(){}
            onWheel: function(){}
        }

        Item {
            id: pausePanel
            width: 620
            height: 620
            anchors.horizontalCenter: parent.horizontalCenter
            y: -height
            z: 1

            Image { anchors.fill: parent; source: "qrc:/image/ui/pauseui.png"; fillMode: Image.PreserveAspectFit; smooth: true }

            Column {
                id: pauseContent
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 250 // 保持整体位置不变
                spacing: 18           // 稍增间距

                Text {
                    text: "游戏暂停";
                    color: "white";
                    font.pixelSize: 24
                    style: Text.Outline
                    styleColor: "black"
                }
                Text {text: "修改初始步数\n重新开始以应用\n";
                    color: "#ffffff";
                    font.pixelSize: 18
                    style: Text.Outline
                    styleColor: "black"
                }
                // 输入区域下移，并保持位于按钮上方
                Row {
                    id: initStepRow
                    spacing: 8
                    anchors.horizontalCenter: parent.horizontalCenter
                    // 可根据需要再往下偏移一点
                    anchors.topMargin: 20

                    Text { id: initStepLabel; text: "初始步数:"; color: "#ffffff"; font.pixelSize: 18
                        style: Text.Outline
                        styleColor: "black"
                    }
                    TextField {
                        id: initStepInput
                        width: 120; height: 32
                        placeholderText: "如 25"
                        validator: IntValidator { bottom: 1; top: 200 }
                        inputMethodHints: Qt.ImhDigitsOnly
                        onAccepted: {
                            var v = parseInt(text);
                            if (!isNaN(v) && mainWindow.gameBoard) mainWindow.gameBoard.init_step = v;
                        }
                        onEditingFinished: {
                            var v = parseInt(text);
                            if (!isNaN(v) && mainWindow.gameBoard) mainWindow.gameBoard.init_step = v;
                        }
                    }
                }

                // 调整：将缓冲高度由16改为6，使下方所有按钮整体上移10px
                Rectangle { width: 1; height: 6; color: "transparent" } // 输入与按钮之间的缓冲（原16）

                Row {
                    id: pauseButtonsRow
                    spacing: 16
                    anchors.horizontalCenter: parent.horizontalCenter
                    Rectangle {
                        width: 110; height: 36; radius: 8; color: "#ff6b6b"
                        Text { anchors.centerIn: parent; text: "重新开始"; color: "white"; font.pixelSize: 15 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                var v = parseInt(initStepInput.text);
                                if (!isNaN(v) && mainWindow.gameBoard) mainWindow.gameBoard.init_step = v;
                                pauseOverlay._pendingRestart = true;
                                pauseOverlay.exitPause();
                            }
                        }
                    }
                    Rectangle {
                        width: 110; height: 36; radius: 8; color: "#4dabf7"
                        Text { anchors.centerIn: parent; text: "继续游戏"; color: "white"; font.pixelSize: 15 }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: {
                                pauseOverlay._pendingRestart = false;
                                pauseOverlay.exitPause();
                            }
                        }
                    }
                }

                // 新增：结束本轮游戏（次级按钮，放在两个大按钮下面，居中）
                Rectangle {
                    id: endRoundBtn
                    width: 180
                    height: 30
                    radius: 6
                    color: "#ffffff"
                    opacity: 0.85
                    anchors.horizontalCenter: parent.horizontalCenter
                    border.color: "#888"
                    border.width: 1

                    Text { anchors.centerIn: parent; text: "结束本轮游戏"; color: "#333"; font.pixelSize: 14 }
                    MouseArea {
                        anchors.fill: parent
                        hoverEnabled: true
                        onEntered: endRoundBtn.opacity = 1.0
                        onExited: endRoundBtn.opacity = 0.85
                        onClicked: {
                            // 直接触发结束界面
                            pauseOverlay._pendingRestart = false;
                            pauseOverlay.exitPause();
                            if (gameOverOverlay && typeof gameOverOverlay.showGameOver === "function") {
                                gameOverOverlay.showGameOver();
                            }
                        }
                    }
                }
            }

            function showPause() {
                pauseOverlay.visible = true;
                pausePanel.y = -pausePanel.height;
                var targetY = (mainWindow.height - pausePanel.height) / 2;
                var anim = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { property: "y"; duration: 450; easing.type: Easing.OutCubic }', pausePanel);
                anim.from = pausePanel.y; anim.to = targetY; anim.target = pausePanel; anim.running = true;
            }
            function exitPause() {
                var fromY = pausePanel.y;
                var toY = -pausePanel.height - 100;
                var anim = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { property: "y"; duration: 380; easing.type: Easing.InCubic }', pausePanel);
                anim.from = fromY; anim.to = toY; anim.target = pausePanel; anim.running = true;
                anim.onStopped.connect(function(){
                    pauseOverlay.visible = false;
                    if (pauseOverlay._pendingRestart && mainWindow.gameBoard) {
                        mainWindow.gameBoard.resetGame();
                    }
                });
            }
        }
    }

    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#4A90E2" }
            GradientStop { position: 1.0; color: "#7B68EE" }
        }

        ColumnLayout {
            anchors.fill: parent
            spacing: 10

            // 标题栏
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 60
                color: "transparent"
                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 20
                    anchors.topMargin: 30
                    Rectangle {
                        width: 100; height: 40; radius: 15; color: "#FF6B6B"
                        Text {
                            anchors.centerIn: parent
                            text: "分数: " + (gameBoard ? gameBoard.score : 0)
                            font.pixelSize: 16
                            color: "white"
                            font.bold: true
                        }
                    }
                }
            }

            // 游戏区域
            Rectangle {
                id: gameArea
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: 10
                color: "#2C3E50"
                radius: 15
                border.color: "#34495E"
                border.width: 3

                property int rows: 8
                property int cols: 8
                property real cellSize: Math.min(width / cols, height / rows)   // 格子尽量正方形
                property real boardWidth: cols * cellSize
                property real boardHeight: rows * cellSize
                property real offsetX: (width - boardWidth) / 2
                property real offsetY: (height - boardHeight) / 2

                Repeater {
                    id: backgroundboardRepeater
                    model: gameArea.rows * gameArea.cols
                    Rectangle {
                        id: backgroundtileRect
                        width: gameArea.cellSize - 2
                        height: gameArea.cellSize - 2
                        radius: 8
                        property int row: Math.floor(index / gameArea.cols)
                        property int col: index % gameArea.cols
                        property real offsetX: 0
                        property real offsetY: 0

                        x: gameArea.offsetX + col * gameArea.cellSize + offsetX
                        y: gameArea.offsetY + row * gameArea.cellSize + offsetY
                        Image {
                            anchors.centerIn: parent
                            width: parent.width * 1
                            height: parent.height * 1
                            fillMode: Image.PreserveAspectFit
                            source: "qrc:/image/item/transparent.png"
                        }
                    }
                }
                Repeater {
                    id: boardRepeater
                    model: gameArea.rows * gameArea.cols

                    Rectangle {
                        id: tileRect
                        width: gameArea.cellSize - 2
                        height: gameArea.cellSize - 2
                        radius: 8
                        border.width: 2
                        border.color: Qt.lighter(color, 1.2)

                        property int row: Math.floor(index / gameArea.cols)
                        property int col: index % gameArea.cols
                        property real offsetX: 0
                        property real offsetY: 0

                        x: gameArea.offsetX + col * gameArea.cellSize + offsetX
                        y: gameArea.offsetY + row * gameArea.cellSize + offsetY

                        property string tileColor: (gameBoard ? (gameBoard.tileAt(row, col) === "" ? "transparent" : gameBoard.tileAt(row, col)) : "gray")
                        property bool isMatched: false
                        color: "transparent"
                        // 注册到动画管理器
                        Component.onCompleted: {
                            if (animManager) {
                                animManager.tileMap[row + "_" + col] = tileRect;
                            }
                        }
                        // 将火箭方块替换为 GIF，其他保持 PNG
                        Item {
                            anchors.centerIn: parent
                            width: parent.width * 0.8
                            height: parent.height * 0.8

                            AnimatedImage {
                                id: tileGif
                                anchors.fill: parent
                                // 直接映射：Rocket_1=竖向GIF，Rocket_2=横向GIF
                                source: (tileRect.tileColor === "Rocket_1")
                                        ? "qrc:/image/item/Rocket_1.gif"
                                        : (tileRect.tileColor === "Rocket_2")
                                            ? "qrc:/image/item/Rocket_2.gif"
                                            : "qrc:/image/item/transparent.png"
                                visible: tileRect.tileColor === "Rocket_1" || tileRect.tileColor === "Rocket_2"
                                playing: visible
                                cache: false
                                fillMode: Image.PreserveAspectFit
                                smooth: true
                                onVisibleChanged: {
                                    if (visible) {
                                        playing = true;
                                    } else {
                                        playing = false;
                                    }
                                }
                                onStatusChanged: {
                                    if (status === Image.Ready && visible) {
                                        playing = true;
                                    }
                                }
                            }

                            Image {
                                anchors.fill: parent
                                // 非火箭时显示静态 PNG；火箭时只给透明占位，避免去加载 Rocket_1.png/Rocket_2.png
                                visible: !(tileRect.tileColor === "Rocket_1" || tileRect.tileColor === "Rocket_2")
                                fillMode: Image.PreserveAspectFit
                                source: (tileRect.tileColor === "Rocket_1" || tileRect.tileColor === "Rocket_2")
                                        ? "qrc:/image/item/transparent.png"
                                        : ("qrc:/image/item/" + tileRect.tileColor + ".png")
                            }
                        }
                        MouseArea {
                            id: mouseArea
                            anchors.fill: parent
                            // 暂停时禁用棋盘交互（双保险：除遮罩拦截外，在控件级别也禁用）
                            enabled: !pauseOverlay.visible

                            property int startRow
                            property int startCol
                            property bool dragging: false

                            onPressed: function(mouse) {
                                dragging = true
                                startRow = tileRect.row
                                startCol = tileRect.col
                                tileRect.scale = 0.9
                            }

                            onReleased: function(mouse) {
                                tileRect.scale = 1.0
                                dragging = false
                            }

                            onClicked: {
                                simpleSfx.playExchangeAudio()
                                var tileColor = gameBoard.tileAt(row, col)
                                console.log("Tile clicked:", row, col, "color: ", tileColor);
                            }
                            onDoubleClicked:{
                                // 双击激活道具
                                var tileColor = gameBoard.tileAt(row, col)
                                if (tileColor === "Rocket_1") {
                                    console.log("播放火箭动画: ", row, col);
                                    animManager.runRocketEffect(row, col, 1);
                                } else if (tileColor === "Rocket_2") {
                                    console.log("播放火箭动画: ", row, col);
                                    animManager.runRocketEffect(row, col, 2);
                                } else if (tileColor === "Bomb") {
                                    console.log("播放炸弹动画: ", row, col);
                                    animManager.runBombEffect(row, col);
                                } else if (tileColor === "SuperItem") {
                                    console.log("播放超级道具动画: ", row, col);
                                    // 修改：仅统计上下左右四邻的常规颜色；若无则全盘随机选一种常规颜色
                                    var colorCount = {};  // 记录颜色出现次数
                                    var mostFrequentColor = ""; // 周围出现次数最多的颜色

                                    // 上下左右四邻
                                    var dirs = [ [ -1, 0 ], [ 1, 0 ], [ 0, -1 ], [ 0, 1 ] ];
                                    for (var i = 0; i < dirs.length; i++) {
                                        var rr = row + dirs[i][0];
                                        var cc = col + dirs[i][1];
                                        if (rr < 0 || rr >= gameArea.rows || cc < 0 || cc >= gameArea.cols) continue;
                                        var color = gameBoard.tileAt(rr, cc);
                                        // 只统计常规颜色，排除空和道具
                                        if (color && color !== "" && color !== "Rocket_1" && color !== "Rocket_2" && color !== "Bomb" && color !== "SuperItem") {
                                            colorCount[color] = (colorCount[color] || 0) + 1;
                                        }
                                    }

                                    var keys = Object.keys(colorCount);
                                    if (keys.length > 0) {
                                        // 四邻里出现次数最多的颜色
                                        mostFrequentColor = keys.reduce(function(a, b) {
                                            return colorCount[a] >= colorCount[b] ? a : b;
                                        });
                                        animManager.runSuperItemEffect(row, col, mostFrequentColor);
                                    } else {
                                        // 四邻无可用颜色：从全盘常规颜色随机选一个作为激活颜色
                                        var allColors = {};
                                        for (var r = 0; r < gameArea.rows; r++) {
                                            for (var c = 0; c < gameArea.cols; c++) {
                                                var v = gameBoard.tileAt(r, c);
                                                if (v && v !== "" && v !== "Rocket_1" && v !== "Rocket_2" && v !== "Bomb" && v !== "SuperItem") {
                                                    allColors[v] = true;
                                                }
                                            }
                                        }
                                        var pool = Object.keys(allColors);
                                        if (pool.length > 0) {
                                            var idx = Math.floor(Math.random() * pool.length);
                                            mostFrequentColor = pool[idx];
                                            console.log("四邻无颜色，随机全盘选择: ", mostFrequentColor);
                                            animManager.runSuperItemEffect(row, col, mostFrequentColor);
                                        } else {
                                            console.log("全盘也无常规颜色，跳过激活");
                                        }
                                    }
                                }
                            }
                            onPositionChanged: function(mouse) {
                                if (!dragging) return;

                                // 计算拖动距离
                                var dx = mouse.x - mouseArea.width/2
                                var dy = mouse.y - mouseArea.height/2

                                // 这里你可以用阈值判断方向
                                if (Math.abs(dx) > cellSize/2 || Math.abs(dy) > cellSize/2) {
                                    var targetRow = startRow + (Math.abs(dy) > Math.abs(dx) ? (dy>0?1:-1) : 0)
                                    var targetCol = startCol + (Math.abs(dx) > Math.abs(dy) ? (dx>0?1:-1) : 0)

                                    // console.log("Drag from", startRow, startCol, "to", targetRow, targetCol)

                                    if (gameBoard) gameBoard.trySwap(startRow, startCol, targetRow, targetCol)
                                    simpleSfx.playExchangeAudio()

                                    dragging = false  // 只处理一次拖动
                                }
                            }
                        }

                        // 匹配动画
                        Behavior on opacity {
                            NumberAnimation { duration: 300 }
                        }

                        Behavior on scale {
                            NumberAnimation { duration: 200 }
                        }
                    }
                }
            }

            // 控制按钮
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 80
                color: "transparent"
                Row {
                    anchors.centerIn: parent
                    spacing: 20
                    Button {
                        text: "开始游戏"
                        onClicked: if (gameBoard) gameBoard.startGame()
                    }
                    Button {
                        text: "重新洗牌"
                        onClicked: if (gameBoard) gameBoard.shuffleBoard()
                    }
                    Button {
                        text: "重新开始"
                        onClicked: if (gameBoard) gameBoard.resetGame()
                    }
                }
            }
        }

        Timer {
            id: animTimer
            interval: 400   // 匹配动画时长
            repeat: false
            onTriggered: {
                // 仅处理匹配，不在此处播放音效，避免重复触发
                gameBoard.processMatches()
            }
        }

        // 连接游戏板信号
        Connections {
            target: gameBoard
            enabled: gameBoard !== undefined

            function onMatchAnimationRequested(matches) {
                console.log("播放匹配动画:", matches)
                if (simpleSfx && simpleSfx.playMatchAudio) simpleSfx.playMatchAudio();
                animTimer.start()
            }
            function onBoardChanged() {
                // 刷新所有tile的颜色并重置视觉状态
                for (var i = 0; i < 64; i++) {
                    var item = boardRepeater.itemAt(i);
                    if (item) {
                        var newColor = gameBoard.tileAt(item.row, item.col);
                        if (newColor === "") newColor = "transparent";
                        item.tileColor = newColor;
                        // 重置可能残留的动画状态，确保视觉与数据一致
                        item.opacity = 1.0;
                        item.scale = 1.0;
                        item.offsetX = 0;
                        item.offsetY = 0;
                    }
                }
            }
            function onComboChanged(comboCount) {
                // 当 C++ 层发送 comboChanged 信号时，更新 QML 层的连击数
                // console.log("comboChanged:", comboCount);
                comboText.text = "连击 x" + comboCount;
                comboAnimation.start();

                // 同步显示图片层
                if (comboCount >= 2) {
                    comboImageLayer.visible = true;
                    comboImageLayer.opacity = 0;
                    comboImageAnim.start();
                    comboImageHideTimer.start();
                    // 播放音效（使用播放器池避免打断）
                    var sfx = getComboSfx(comboCount);
                    playComboSfx(sfx);
                }
            }

            function onInvalidSwap(r1, c1, r2, c2) {
                // console.log("无效交换:", r1, c1, r2, c2);
                // 播放抖动动画
                var tile1 = animManager.findTile(r1, c1);
                var tile2 = animManager.findTile(r2, c2);
                if (tile1) animManager.shakeTile(tile1);
                if (tile2) animManager.shakeTile(tile2);
            }

            function onBombCreateRequested(bombMatches) {
                console.log("生成炸弹动画: ", bombMatches);
                // runBombEffect(bombMatches);
            }

            function onRocketCreateRequested(rocketMatches) {
                console.log("生成火箭动画: ", rocketMatches);
                // animManager.runRocketEffect(rocketMatches);
            }

           function onSuperItemCreateRequested(superItemMatches) {
                console.log("生成超级道具动画: ", superItemMatches);
                // runSuperItemEffect(superItemMatches);
            }

            function onPropEffect(row, col, type, color){
                if (type === 1) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭激活动画: ", row, col);
                    animManager.runRocketEffect(row, col, 1);
                } else if (type === 2) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭激活动画: ", row, col);
                    animManager.runRocketEffect(row, col, 2);
                } else if (type === 3) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("bomb");
                    console.log("播放炸弹激活动画: ", row, col);
                    animManager.runBombEffect(row, col);
                } else if (type === 4) {
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级道具激活动画: ", row, col);
                    animManager.runSuperItemEffect(row, col, color);
                } else if (type === 100) { // Rocket + Rocket combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放火箭+火箭激活动画:", row, col);
                    animManager.runComboRocketRocket(row, col);
                } else if (type === 101) { // Bomb + Bomb combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("bomb");
                    console.log("播放炸弹+炸弹激活动画:", row, col);
                    animManager.runComboBombBomb(row, col);
                } else if (type === 102) // Bomb + Rocket combo
                {
                    // 修复：炸弹+火箭的音效改为火箭音效，与单独火箭一致
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("rocket");
                    console.log("播放炸弹+火箭激活动画:", row, col, "meta:", color);
                    var rocketType = parseInt(color);
                    animManager.runComboBombRocket(row, col, rocketType);
                } else if (type === 103) { // Super + Bomb combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+炸弹激活动画:", row, col);
                    animManager.runComboSuperBomb(row, col);
                } else if (type === 104) {// Super + Rocket combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+火箭激活动画:", row, col);
                    animManager.runComboSuperRocket(row, col);
                } else if (type === 105) { // Super + Super combo
                    if (simpleSfx && simpleSfx.playPropLimited) simpleSfx.playPropLimited("super");
                    console.log("播放超级+超级激活涟漪动画:", row, col);
                    animManager.runComboSuperSuper(row, col);
                }
            }
        }
    }

    // 动画管理器
    AnimationManager {
        id: animManager
        boardView: gameArea
        gameBoard: gameBoardCpp
    }

    // 简易音效模块：新增并发上限（每 250ms 最多播放 3 次道具音效）
    Item {
        id: simpleSfx
        property real volume: 0.8
        SoundEffect { id: exchangeSfx;  source: "qrc:/music/Exchange.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match1Sfx; source: "qrc:/music/match1.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match2Sfx; source: "qrc:/music/match2.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match3Sfx; source: "qrc:/music/match3.wav"; volume: simpleSfx.volume }
        SoundEffect { id: match4Sfx; source: "qrc:/music/match4.wav"; volume: simpleSfx.volume }
        SoundEffect { id: drop1Sfx;  source: "qrc:/music/drop1.wav"; volume: simpleSfx.volume }
        SoundEffect { id: drop2Sfx;  source: "qrc:/music/drop2.wav"; volume: simpleSfx.volume }
        SoundEffect { id: rocketSfx;  source: "qrc:/music/rocket.wav"; volume: simpleSfx.volume }
        SoundEffect { id: bombSfx;    source: "qrc:/music/Bomb.wav"; volume: simpleSfx.volume }
        SoundEffect { id: superItemSfx;  source: "qrc:/music/SuperItem.wav"; volume: simpleSfx.volume }

        property int propBurstCount: 0
        property double _burstWindowStart: 0
        property double _now: 0
        property double rocketLastTs: 0
        property double bombLastTs: 0
        property double superLastTs: 0
        property int burstWindowMs: 250
        property int kindCooldownMs: 120
        Timer {
            id: propBurstTick
            interval: 50; repeat: true; running: true
            onTriggered: simpleSfx._now = Date.now()
        }

        function _resetWindow(nowTs) {
            _burstWindowStart = nowTs; propBurstCount = 0;
        }

        function playPropLimited(kind) {
            var nowTs = (simpleSfx._now > 0 ? simpleSfx._now : Date.now());
            if (nowTs - _burstWindowStart > burstWindowMs) {
                _resetWindow(nowTs);
            }
            if (propBurstCount >= 3) return; // 全局窗口上限

            // 同类短冷却，避免同一声音快速连击
            var lastTs = 0;
            if (kind === "rocket") lastTs = rocketLastTs;
            else if (kind === "bomb") lastTs = bombLastTs;
            else if (kind === "super") lastTs = superLastTs;
            if (nowTs - lastTs < kindCooldownMs) return;

            // 适度降音防爆音
            var prevVol = simpleSfx.volume;
            if (propBurstCount >= 2) simpleSfx.volume = Math.max(0.6, prevVol * 0.8);

            // 播放
            if (kind === "rocket") { rocketSfx.play(); rocketLastTs = nowTs; }
            else if (kind === "bomb") { bombSfx.play(); bombLastTs = nowTs; }
            else if (kind === "super") { superItemSfx.play(); superLastTs = nowTs; }
            propBurstCount++;

            // 恢复音量（异步，以免影响其他 SoundEffect 混音）
            Qt.callLater(function(){ simpleSfx.volume = prevVol; });
        }

        function playExchangeAudio() {
            exchangeSfx.play()
        }
        function playMatchAudio() {
            // 改用 MediaPlayer 池播放，避免 SoundEffect 在当前环境不出声的问题
            var r = Math.floor(Math.random() * 4) + 1;
            var url = (r === 1) ? "qrc:/music/match1.wav"
                      : (r === 2) ? "qrc:/music/match2.wav"
                      : (r === 3) ? "qrc:/music/match3.wav"
                      : "qrc:/music/match4.wav";
            if (mainWindow && mainWindow.playComboSfx) mainWindow.playComboSfx(url);
        }

    }

    // 结束画面覆盖层
    Item {
        id: gameOverOverlay
        anchors.fill: parent
        visible: false
        z: 9998
        property bool _pendingRestart: false

        // 背景半透明灰色
        Rectangle {
            anchors.fill: parent
            color: "#88000000"
            visible: true
        }

        // 拦截所有输入，避免底层被操作
        MouseArea {
            id: overlayBlocker
            anchors.fill: parent
            hoverEnabled: true
            propagateComposedEvents: false
            enabled: true
            onPressed: function(){}
            onWheel: function(){}
            onReleased: function(){}
        }

        // 容器（用于整体上下滑动动画）
        Item {
            id: gameOverPanel
            width: 500
            height: 500
            anchors.horizontalCenter: parent.horizontalCenter
            y: -height

            Image {
                anchors.fill: parent
                source: "qrc:/image/ui/gameover.png"
                fillMode: Image.PreserveAspectFit
                smooth: true
            }

            // 分数显示（置顶，居中加大字号）
            Rectangle {
                id: scoreHeader
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: parent.top
                anchors.topMargin: 60
                width: parent.width
                height: 80
                color: "transparent"
                Text {
                    anchors.centerIn: parent
                    text: "初始步数：" + (mainWindow.gameBoard && mainWindow.gameBoard.init_step !== undefined ? mainWindow.gameBoard.init_step : 0) +
                          "\n最终得分：" + (mainWindow.gameBoard ? mainWindow.gameBoard.score : 0)
                    font.pixelSize: 28
                    font.bold: true
                    color: "#ffffff"
                    // 黑色描边
                    style: Text.Outline
                    styleColor: "black"
                    horizontalAlignment: Text.AlignHCenter
                }
            }

            // 统计展示区域（放在分数显示下面）
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.top: scoreHeader.bottom
                anchors.topMargin: 40
                spacing: 30

                // 左列：六种颜色
                Column {
                    spacing: 8
                    Repeater {
                        model: ["red","green","blue","yellow","purple","brown"]
                        Row {
                            spacing: 6
                            Image { source: "qrc:/image/item/" + modelData + ".png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                            Text {
                                // 改为绑定 stats 数组（索引与六色顺序一致）
                                text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[index] : 0)
                                color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black"
                            }
                        }
                    }
                }

                // 中列：三种普通道具
                Column {
                    spacing: 8
                    Row {
                        spacing: 6
                        AnimatedImage { source: "qrc:/image/item/Rocket_1.gif"; width: 28; height: 28; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[6] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/Bomb.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[7] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/SuperItem.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[8] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                }
                // 右列 所有组合道具
                Column {
                    spacing: 8
                    Row {
                        spacing: 6
                        AnimatedImage { source: "qrc:/image/item/Rocket_1.gif"; width: 24; height: 24; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }
                        AnimatedImage { source: "qrc:/image/item/Rocket_2.gif"; width: 24; height: 24; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[9] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/Bomb.png"; width: 24; height: 24; fillMode: Image.PreserveAspectFit }
                        Image { source: "qrc:/image/item/Bomb.png"; width: 24; height: 24; fillMode: Image.PreserveAspectFit }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[10] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/Bomb.png"; width: 24; height: 24; fillMode: Image.PreserveAspectFit }
                        AnimatedImage { source: "qrc:/image/item/Rocket_1.gif"; width: 24; height: 24; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[11] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/SuperItem.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        AnimatedImage { source: "qrc:/image/item/Rocket_1.gif"; width: 24; height: 24; playing: true; cache: false; fillMode: Image.PreserveAspectFit; smooth: true }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[12] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/SuperItem.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Image { source: "qrc:/image/item/Bomb.png"; width: 24; height: 24; fillMode: Image.PreserveAspectFit }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[13] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                    Row {
                        spacing: 6
                        Image { source: "qrc:/image/item/SuperItem.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Image { source: "qrc:/image/item/SuperItem.png"; width: 28; height: 28; fillMode: Image.PreserveAspectFit }
                        Text { text: "× " + (mainWindow.gameBoard && mainWindow.gameBoard.stats ? mainWindow.gameBoard.stats[14] : 0); color: "#ffffff"; font.pixelSize: 16; style: Text.Outline; styleColor: "black" }
                    }
                }
            }

            // 按钮区域
            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 28
                spacing: 24

                Rectangle {
                    id: btnRestart
                    width: 160; height: 44
                    radius: 8
                    color: "#ff6b6b"
                    Text { anchors.centerIn: parent; text: "重新开始"; color: "white"; font.pixelSize: 18 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            gameOverOverlay._pendingRestart = true;
                            gameOverOverlay.exitGameOver();
                        }
                    }
                }
                Rectangle {
                    id: btnFree
                    width: 160; height: 44
                    radius: 8
                    color: "#4dabf7"
                    Text { anchors.centerIn: parent; text: "自由游戏"; color: "white"; font.pixelSize: 18 }
                    MouseArea {
                        anchors.fill: parent
                        onClicked: {
                            gameOverOverlay._pendingRestart = false;
                            gameOverOverlay.exitGameOver();
                        }
                    }
                }
            }
        }

        function showGameOver() {
            visible = true;
            overlayBlocker.enabled = true;
            gameOverPanel.y = -gameOverPanel.height;
            var targetY = (mainWindow.height - gameOverPanel.height) / 2;
            var anim = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { property: "y"; duration: 450; easing.type: Easing.OutCubic }', gameOverPanel);
            anim.from = gameOverPanel.y;
            anim.to = targetY;
            anim.target = gameOverPanel;
            anim.running = true;
        }
        function exitGameOver() {
            var fromY = gameOverPanel.y;
            var toY = -gameOverPanel.height - 100;
            var anim = Qt.createQmlObject('import QtQuick 2.15; NumberAnimation { property: "y"; duration: 380; easing.type: Easing.InCubic }', gameOverPanel);
            anim.from = fromY; anim.to = toY; anim.target = gameOverPanel;
            anim.running = true;
            anim.onStopped.connect(function(){
                // 关闭遮罩并恢复底层交互
                overlayBlocker.enabled = false;
                gameOverOverlay.visible = false;
                if (gameOverOverlay._pendingRestart && mainWindow.gameBoard) {
                    mainWindow.gameBoard.resetGame();
                }
            });
        }
    }

    // 监听后端 gameOver 信号
    Connections {
        target: mainWindow.gameBoard
        function onGameOver() {
            gameOverOverlay._pendingRestart = false;
            gameOverOverlay.showGameOver();
        }
    }


}
