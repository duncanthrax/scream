#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QFile>
#include <QMainWindow>
#include <QTimer>
#include <QtNetwork/QUdpSocket>

#include "ac3encoder.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
    void on_startButton_clicked();
    void on_stopButton_clicked();
    void on_bitrateSlider_valueChanged(int value);
    void on_samplerateSlider_valueChanged(int value);
    void on_sendIntervalSlider_valueChanged(int value);

private:
    void onTimeout();

    Ui::MainWindow *ui;
    QTimer m_timer;
    QUdpSocket m_socket;
    Ac3Encoder m_encoder;
    int m_samplerate = 48000;
    int m_sendInterval = 32;
    int m_testFrequency = 440;
};

#endif // MAINWINDOW_H
