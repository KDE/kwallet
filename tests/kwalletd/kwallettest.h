#ifndef KWALLETTEST_H
#define KWALLETTEST_H

#include <QObject>

class WalletReceiver : public QObject
{
    Q_OBJECT
public Q_SLOTS:
    void walletOpened(bool);
};

#endif // KWALLETTEST_H
