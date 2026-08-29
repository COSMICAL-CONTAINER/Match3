QT += core gui qml quick quickcontrols2 multimedia
# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        GameBoard.cpp \
        core/Board.cpp \
        core/BoardProp.cpp \
        core/BoardTile.cpp \
        core/GameBoardCompat.cpp \
        core/Level.cpp \
        core/Match3Game.cpp \
        core/MatchFinder.cpp \
        core/MatchFinderBridge.cpp \
        core/BoardModel.cpp \
        core/GameEngine.cpp \
        core/PlayerProps.cpp \
        main.cpp

RESOURCES += qml.qrc

TRANSLATIONS += \
    Match3Demo_zh_CN.ts
CONFIG += lrelease
CONFIG += embed_translations

# Additional import path used to resolve QML modules in Qt Creator's code model
QML_IMPORT_PATH =

# Additional import path used to resolve QML modules just for Qt Quick Designer
QML_DESIGNER_IMPORT_PATH =

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

HEADERS += \
    GameBoard.h \
    core/Board.h \
    core/BoardItemBase.h \
    core/BoardObstacle.h \
    core/BoardProp.h \
    core/BoardTile.h \
    core/GameBoardCompat.h \
    core/Level.h \
    core/Match3Game.h \
    core/MatchFinder.h \
    core/MatchFinderBridge.h \
    core/PlayerProps.h \
    core/BoardModel.h \
    core/GameEngine.h \
    core/Types.h \
    core/Types.h

