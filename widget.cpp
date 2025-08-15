#include "widget.h"
#include "ui_widget.h"
#include <QMessageBox>
#include <QFileDialog>
#include <QSerialPortInfo>
#include <QDateTime>
#include <QTextCursor>
#include <QThread>

Widget::Widget(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::Widget),
    serialPort(new QSerialPort),
    ymodemFileTransmit(new YmodemFileTransmit),
    bootloaderWaitTimer(new QTimer),
    logFile(new QFile),
    logStream(new QTextStream)
{
    transmitButtonStatus = false;
    waitingForBootloader = false;

    ui->setupUi(this);

    QSerialPortInfo serialPortInfo;

    foreach(serialPortInfo, QSerialPortInfo::availablePorts())
    {
        ui->comPort->addItem(serialPortInfo.portName());
    }

    serialPort->setPortName("COM1");
    serialPort->setBaudRate(115200);
    serialPort->setDataBits(QSerialPort::Data8);
    serialPort->setStopBits(QSerialPort::OneStop);
    serialPort->setParity(QSerialPort::NoParity);
    serialPort->setFlowControl(QSerialPort::NoFlowControl);

    connect(ymodemFileTransmit, SIGNAL(transmitProgress(int)), this, SLOT(transmitProgress(int)));
    connect(ymodemFileTransmit, SIGNAL(transmitStatus(YmodemFileTransmit::Status)), this, SLOT(transmitStatus(YmodemFileTransmit::Status)));
    connect(bootloaderWaitTimer, SIGNAL(timeout()), this, SLOT(onWaitForBootloaderTimeout()));
    connect(serialPort, SIGNAL(readyRead()), this, SLOT(onSerialDataReceived()));
    
    // 设置bootloader等待超时时间为10秒
    bootloaderWaitTimer->setSingleShot(true);
    bootloaderWaitTimer->setInterval(10000);
    
    // 初始化日志系统
    initializeLogging();
}

Widget::~Widget()
{
    // 关闭日志文件
    if(logFile && logFile->isOpen())
    {
        logFile->close();
    }
    
    delete ui;
    delete serialPort;
    delete ymodemFileTransmit;
    delete bootloaderWaitTimer;
    delete logStream;
    delete logFile;
}

void Widget::on_comButton_clicked()
{
    static bool button_status = false;

    if(button_status == false)
    {
        serialPort->setPortName(ui->comPort->currentText());
        serialPort->setBaudRate(ui->comBaudRate->currentText().toInt());

        if(serialPort->open(QSerialPort::ReadWrite) == true)
        {
            button_status = true;
            appendLog(QString("串口打开成功: %1 - %2").arg(ui->comPort->currentText(), ui->comBaudRate->currentText()));

            ui->comPort->setDisabled(true);
            ui->comBaudRate->setDisabled(true);
            ui->comButton->setText(u8"关闭串口");

            ui->transmitBrowse->setEnabled(true);

            if(ui->transmitPath->text().isEmpty() != true)
            {
                ui->transmitButton->setEnabled(true);
            }
        }
        else
        {
            appendLog(QString("串口打开失败: %1").arg(ui->comPort->currentText()));
            QMessageBox::warning(this, u8"串口打开失败", u8"请检查串口是否已被占用！", u8"关闭");
        }
    }
    else
    {
        button_status = false;
        appendLog("串口关闭");

        serialPort->close();

        ui->comPort->setEnabled(true);
        ui->comBaudRate->setEnabled(true);
        ui->comButton->setText(u8"打开串口");

        ui->transmitBrowse->setDisabled(true);
        ui->transmitButton->setDisabled(true);
    }
}

void Widget::on_transmitBrowse_clicked()
{
    QString fileName = QFileDialog::getOpenFileName(this, u8"选择固件文件", ".", u8"固件文件 (*.bin *.hex *.*)");
    ui->transmitPath->setText(fileName);

    if(ui->transmitPath->text().isEmpty() != true)
    {
        appendLog(QString("选择固件文件: %1").arg(fileName));
        ui->transmitButton->setEnabled(true);
    }
    else
    {
        ui->transmitButton->setDisabled(true);
    }
}



void Widget::appendLog(const QString &message)
{
    // 只写入文件，不在界面显示
    if(logStream && logFile && logFile->isOpen())
    {
        QString timestamp = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss.zzz");
        QString logEntry = QString("[%1] %2").arg(timestamp, message);
        
        *logStream << logEntry << Qt::endl;
        logStream->flush(); // 立即写入文件
    }
}

void Widget::on_transmitButton_clicked()
{
    if(transmitButtonStatus == false)
    {
        if(waitingForBootloader)
        {
            // 取消等待bootloader
            bootloaderWaitTimer->stop();
            waitingForBootloader = false;
            ui->transmitButton->setText(u8"开始升级");
            return;
        }
        
        // 开始升级流程：先发送upgrade命令
        appendLog("=== 开始固件升级流程 ===");
        transmitButtonStatus = true;
        ui->comButton->setDisabled(true);
        ui->transmitBrowse->setDisabled(true);
        ui->transmitButton->setText(u8"取消升级");
        ui->transmitProgress->setValue(0);
        
        sendUpgradeCommand();
    }
    else
    {
        // 取消升级
        appendLog("用户取消升级操作");
        if(waitingForBootloader)
        {
            bootloaderWaitTimer->stop();
            waitingForBootloader = false;
        }
        else
        {
            ymodemFileTransmit->stopTransmit();
        }
        
        transmitButtonStatus = false;
        ui->comButton->setEnabled(true);
        ui->transmitBrowse->setEnabled(true);
        ui->transmitButton->setText(u8"开始升级");
    }
}



void Widget::transmitProgress(int progress)
{
    ui->transmitProgress->setValue(progress);
}



void Widget::transmitStatus(Ymodem::Status status)
{
    switch(status)
    {
        case YmodemFileTransmit::StatusEstablish:
        {
            break;
        }

        case YmodemFileTransmit::StatusTransmit:
        {
            break;
        }

        case YmodemFileTransmit::StatusFinish:
        {
            appendLog("=== 固件升级完成 ===");
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"开始升级");

            QMessageBox::information(this, u8"成功", u8"固件升级成功！", u8"关闭");

            break;
        }

        case YmodemFileTransmit::StatusAbort:
        {
            appendLog("固件升级被中止");
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"开始升级");

            QMessageBox::warning(this, u8"失败", u8"固件升级失败！", u8"关闭");

            break;
        }

        case YmodemFileTransmit::StatusTimeout:
        {
            appendLog("固件升级超时");
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"开始升级");

            QMessageBox::warning(this, u8"失败", u8"固件升级超时！", u8"关闭");

            break;
        }

        default:
        {
            appendLog("固件升级遇到未知错误");
            transmitButtonStatus = false;

            ui->comButton->setEnabled(true);

            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"开始升级");

            QMessageBox::warning(this, u8"失败", u8"固件升级失败！", u8"关闭");
        }
    }
}

void Widget::sendUpgradeCommand()
{
    appendLog("开始升级流程...");
    
    // 确保串口是打开的
    if(!serialPort->isOpen())
    {
        serialPort->setPortName(ui->comPort->currentText());
        serialPort->setBaudRate(ui->comBaudRate->currentText().toInt());
        
        appendLog(QString("正在打开串口: %1, 波特率: %2").arg(ui->comPort->currentText()).arg(ui->comBaudRate->currentText()));
        
        if(!serialPort->open(QSerialPort::ReadWrite))
        {
            appendLog("错误: 无法打开串口！");
            QMessageBox::warning(this, u8"错误", u8"无法打开串口！", u8"关闭");
            transmitButtonStatus = false;
            ui->comButton->setEnabled(true);
            ui->transmitBrowse->setEnabled(true);
            ui->transmitButton->setText(u8"开始升级");
            return;
        }
        
        appendLog("串口打开成功");
    }
    
    // 清空接收缓冲区
    serialPort->clear();
    
    // 首先尝试慢速发送
    sendUpgradeCommandSlow();
}

void Widget::sendUpgradeCommandSlow()
{
    // 使用逐字符慢速发送模式
    QByteArray upgradeCommand = "upgrade\r\n";
    // appendLog(QString("发送upgrade命令: %1 (十六进制: %2)").arg(QString(upgradeCommand)).arg(QString(upgradeCommand.toHex(' '))));
    appendLog("使用慢速逐字符发送模式（每字符间隔10ms）...");
    
    qint64 totalBytesWritten = 0;
    
    for(int i = 0; i < upgradeCommand.size(); i++)
    {
        char singleChar = upgradeCommand.at(i);
        qint64 bytesWritten = serialPort->write(&singleChar, 1);
        
        if(bytesWritten == 1)
        {
            totalBytesWritten++;
            serialPort->waitForBytesWritten(100); // 等待字符发送完成
            
            // 显示当前发送的字符（用于调试）
            if(singleChar == '\r')
                appendLog(QString("发送字符 %1: \\r").arg(i+1));
            else if(singleChar == '\n')
                appendLog(QString("发送字符 %1: \\n").arg(i+1));
            else
                appendLog(QString("发送字符 %1: %2").arg(i+1).arg(singleChar));
            
            // 在字符之间添加延时
            if(i < upgradeCommand.size() - 1)
            {
                QThread::msleep(10); // 10ms延时
            }
        }
        else
        {
            appendLog(QString("错误: 发送第%1个字符失败").arg(i+1));
            return;
        }
    }
    
    appendLog(QString("慢速发送完成，总共发送 %1 字节").arg(totalBytesWritten));
    
    // 短暂等待让MCU处理命令
    QTimer::singleShot(1000, [this]() {
        waitingForBootloader = true;
        bootloaderWaitTimer->start();
        appendLog("等待MCU进入bootloader模式...");
        ui->transmitButton->setText(u8"等待MCU进入升级模式...");
    });
}

void Widget::onWaitForBootloaderTimeout()
{
    bootloaderWaitTimer->stop();
    waitingForBootloader = false;
    transmitButtonStatus = false;
    
    appendLog("超时: 等待MCU进入bootloader模式超时！");
    appendLog("可能的原因: 1) MCU未连接 2) 升级命令格式不正确 3) MCU固件不支持升级命令");
    
    ui->comButton->setEnabled(true);
    ui->transmitBrowse->setEnabled(true);
    ui->transmitButton->setText(u8"开始升级");
    
    QMessageBox::warning(this, u8"超时", u8"等待MCU进入升级模式超时！请检查MCU连接和状态。", u8"关闭");
}

void Widget::onSerialDataReceived()
{
    QByteArray data = serialPort->readAll();
    
    if(!data.isEmpty())
    {
        // 记录所有接收到的数据
        QString receivedText = QString::fromUtf8(data);
        QString hexData = data.toHex(' ');
        // appendLog(QString("接收到数据: \"%1\" (十六进制: %2)").arg(receivedText).arg(hexData));
        
        if(waitingForBootloader)
        {
            // 检查是否收到'C'字符，表示bootloader已准备好接收数据
            if(data.contains('C'))
            {
                appendLog("检测到bootloader准备信号 'C'，开始发送固件...");
                bootloaderWaitTimer->stop();
                waitingForBootloader = false;
                
                // 开始发送固件
                startFirmwareTransmission();
            }
            else
            {
                // 检查是否有其他响应
                if(data.contains("OK") || data.contains("ok"))
                {
                    appendLog("MCU响应OK，继续等待bootloader信号...");
                }
                else if(data.contains("ERROR") || data.contains("error"))
                {
                    appendLog("MCU响应ERROR，升级命令可能不被识别");
                }
                else if(data.contains("upgrade") || data.contains("UPGRADE"))
                {
                    appendLog("MCU已确认upgrade命令，等待进入bootloader...");
                }
            }
        }
    }
}

void Widget::startFirmwareTransmission()
{
    appendLog("MCU已进入bootloader模式，开始发送固件文件...");
    
    // 关闭串口，让YmodemFileTransmit重新打开
    serialPort->close();
    appendLog("关闭调试串口，准备启动YMODEM传输");
    
    ymodemFileTransmit->setFileName(ui->transmitPath->text());
    ymodemFileTransmit->setPortName(ui->comPort->currentText());
    ymodemFileTransmit->setPortBaudRate(ui->comBaudRate->currentText().toInt());

    appendLog(QString("准备发送固件文件: %1").arg(ui->transmitPath->text()));

    if(ymodemFileTransmit->startTransmit() == true)
    {
        appendLog("YMODEM传输已启动");
        ui->transmitButton->setText(u8"取消升级");
    }
    else
    {
        appendLog("错误: YMODEM传输启动失败！");
        transmitButtonStatus = false;
        ui->comButton->setEnabled(true);
        ui->transmitBrowse->setEnabled(true);
        ui->transmitButton->setText(u8"开始升级");
        QMessageBox::warning(this, u8"失败", u8"固件升级失败！", u8"关闭");
    }
}

void Widget::initializeLogging()
{
    // 创建日志文件名（包含时间戳）
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString logFileName = QString("upgrade_log_%1.txt").arg(timestamp);
    
    logFile->setFileName(logFileName);
    
    if(logFile->open(QIODevice::WriteOnly | QIODevice::Append))
    {
        logStream->setDevice(logFile);
        // Qt6中不再需要setCodec，默认使用UTF-8
        
        appendLog("=== MCU固件升级工具启动 ===");
        appendLog(QString("日志文件: %1").arg(logFileName));
    }
}


