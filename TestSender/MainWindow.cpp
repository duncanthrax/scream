#include "MainWindow.h"
#include "ui_MainWindow.h"

#include <QFile>
#include <QTimer>
#include <QtMath>

#include <array>

#include "rtpheader.h"

static RtpHeader s_rtpheader;
static uint16_t s_seq;
static uint32_t s_ts;

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    connect(&m_timer, &QTimer::timeout, this, &MainWindow::onTimeout);
    m_encoder.Initialize(m_samplerate, 2);

    s_rtpheader.v = 2;
    s_rtpheader.m = 1;
    s_rtpheader.pt = 96;
    s_ts = rand();
    s_seq = rand();
    s_rtpheader.ssrc = rand();
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::on_startButton_clicked()
{
    m_timer.start(m_sendInterval);
}

void MainWindow::on_stopButton_clicked()
{
    m_timer.stop();
}

void MainWindow::on_bitrateSlider_valueChanged(int value)
{
    static std::array<uint, 8> rates { 128, 160, 192, 224, 256, 320, 384, 448 };
    ui->bitrateReadout->setText(QString::number(rates[value]));
    m_encoder.SetBitrate(rates[value]);
}

void MainWindow::on_samplerateSlider_valueChanged(int value)
{
    static std::array<uint, 2> rates { 44100, 48000 };
    m_samplerate = rates[value];
    ui->samplerateReadout->setText(QString::number(rates[value]/1000.0, 'g', 3));
    m_encoder.Initialize(rates[value], 2);
}

void MainWindow::on_sendIntervalSlider_valueChanged(int value)
{
    m_sendInterval = value;
    ui->sendIntervalReadout->setText(QString::number(value));
    m_timer.start(m_sendInterval);
}

void MainWindow::onTimeout()
{
    static uint32_t i = 0;
    std::array<int16_t, 1536*2> samples;
    for (size_t j = 0; j < samples.size(); j += 2) {
        samples[j] = 16384*sin(m_testFrequency * 2 * M_PI * ++i / m_samplerate);
        samples[j+1] = samples[j];
    }

    if (auto sample = m_encoder.Process(samples.data(), samples.size()*2)) {
        s_rtpheader.seq = _byteswap_ushort(s_seq);
        s_rtpheader.ts = _byteswap_ulong(s_ts);

        IMFMediaBuffer* buffer = nullptr;
        sample->GetBufferByIndex(0, &buffer);
        BYTE *ac3Data;
        DWORD maxAc3Length, currentAc3Length;
        buffer->Lock(&ac3Data, &maxAc3Length, &currentAc3Length);
        QByteArray rtpPacket;
        rtpPacket.reserve(sizeof(RtpHeader) + 2 + currentAc3Length);
        rtpPacket.append((const char*)&s_rtpheader, sizeof(RtpHeader));
        rtpPacket.append(char(0));
        rtpPacket.append(char(1));
        rtpPacket.append((const char*)ac3Data, currentAc3Length);
        buffer->Unlock();
        m_encoder.ReleaseSample(sample);

        m_socket.writeDatagram(rtpPacket, QHostAddress("239.255.77.77"), 4010);

        s_seq++;
        s_ts += 1536;
    }
}
