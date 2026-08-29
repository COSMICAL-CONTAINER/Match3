#pragma once
#include <QObject>

// PlayerProps: 玩家携带道具/消耗品库存
class PlayerProps : public QObject {
    Q_OBJECT
public:
    struct Inventory { int hammer = 0; int rowRocket = 0; int colRocket = 0; int shuffle = 0; };
    explicit PlayerProps(QObject *parent = nullptr);

    Inventory inventory() const { return m_inv; }

    // 消费接口，使用失败返回 false
    Q_INVOKABLE bool useHammer();
    Q_INVOKABLE bool useRowRocket();
    Q_INVOKABLE bool useColRocket();
    Q_INVOKABLE bool useShuffle();

    Q_INVOKABLE void addHammer(int n);
    Q_INVOKABLE void addRowRocket(int n);
    Q_INVOKABLE void addColRocket(int n);
    Q_INVOKABLE void addShuffle(int n);

signals:
    void inventoryChanged();

private:
    Inventory m_inv;
};
