#ifndef WIDGET_H
#define WIDGET_H

#include <QWidget>
#include <QSerialPort>
#include <QTimer>
#include <QTextStream>
#include <QFile>
#include <QDateTime>
#include "YmodemFileTransmit.h"


namespace Ui {
class Widget;
}

class Widget : public QWidget
{
    Q_OBJECT

public:
    explicit Widget(QWidget *parent = 0);
    ~Widget();

private slots:
    void on_comButton_clicked();
    void on_transmitBrowse_clicked();
    void on_transmitButton_clicked();
    void on_directBurnButton_clicked();
    void transmitProgress(int progress);
    void transmitStatus(YmodemFileTransmit::Status status);
    void onWaitForBootloaderTimeout();
    void onSerialDataReceived();

private:
    Ui::Widget *ui;
    QSerialPort *serialPort;
    YmodemFileTransmit *ymodemFileTransmit;
    QTimer *bootloaderWaitTimer;
    QFile *logFile;
    QTextStream *logStream;

    bool transmitButtonStatus;
    bool directBurnButtonStatus;
    bool waitingForBootloader;
    
    void sendUpgradeCommand();
    void sendUpgradeCommandSlow();
    void startFirmwareTransmission();
    void startDirectFirmwareTransmission();
    void appendLog(const QString &message);
    void initializeLogging();
};

#endif // WIDGET_H
