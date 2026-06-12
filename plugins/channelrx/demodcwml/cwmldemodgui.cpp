///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 morse-pro contributors                                     //
//                                                                               //
// This program is free software; you can redistribute it and/or modify          //
// it under the terms of the GNU General Public License as published by          //
// the Free Software Foundation as version 3 of the License, or                  //
// (at your option) any later version.                                           //
//                                                                               //
// This program is distributed in the hope that it will be useful,               //
// but WITHOUT ANY WARRANTY; without even the implied warranty of                //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the                  //
// GNU General Public License V3 for more details.                               //
//                                                                               //
// You should have received a copy of the GNU General Public License             //
// along with this program. If not, see <http://www.gnu.org/licenses/>.          //
///////////////////////////////////////////////////////////////////////////////////

#include <QDebug>
#include <QFileDialog>
#include <QFileInfo>
#include <QScrollBar>

#include "cwmldemodgui.h"

#include "device/deviceuiset.h"
#include "dsp/dspcommands.h"
#include "ui_cwmldemodgui.h"
#include "plugin/pluginapi.h"
#include "util/db.h"
#include "gui/basicchannelsettingsdialog.h"
#include "gui/dialogpositioner.h"
#include "maincore.h"

#include "cwmldemod.h"

CWMLDemodGUI* CWMLDemodGUI::create(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel)
{
    CWMLDemodGUI* gui = new CWMLDemodGUI(pluginAPI, deviceUISet, rxChannel);
    return gui;
}

void CWMLDemodGUI::destroy()
{
    delete this;
}

void CWMLDemodGUI::resetToDefaults()
{
    m_settings.resetToDefaults();
    displaySettings();
    applySettings(QStringList(), true);
}

QByteArray CWMLDemodGUI::serialize() const
{
    return m_settings.serialize();
}

bool CWMLDemodGUI::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data)) {
        displaySettings();
        applySettings(QStringList(), true);
        return true;
    } else {
        resetToDefaults();
        return false;
    }
}

void CWMLDemodGUI::textReceived(const QString& text)
{
    // Is the scroll bar at the bottom?
    int scrollPos = ui->text->verticalScrollBar()->value();
    bool atBottom = scrollPos >= ui->text->verticalScrollBar()->maximum();

    // Move cursor to end where we want to append new text
    // (user may have moved it by clicking / highlighting text)
    ui->text->moveCursor(QTextCursor::End);

    // Restore scroll position
    ui->text->verticalScrollBar()->setValue(scrollPos);

    ui->text->insertPlainText(text);

    // Scroll to bottom, if we we're previously at the bottom
    if (atBottom) {
        ui->text->verticalScrollBar()->setValue(ui->text->verticalScrollBar()->maximum());
    }
}

bool CWMLDemodGUI::handleMessage(const Message& message)
{
    if (CWMLDemod::MsgConfigureCWMLDemod::match(message))
    {
        qDebug("CWMLDemodGUI::handleMessage: CWMLDemod::MsgConfigureCWMLDemod");
        const CWMLDemod::MsgConfigureCWMLDemod& cfg = (CWMLDemod::MsgConfigureCWMLDemod&) message;
        m_settings = cfg.getSettings();
        blockApplySettings(true);
        m_channelMarker.updateSettings(static_cast<const ChannelMarker*>(m_settings.m_channelMarker));
        displaySettings();
        blockApplySettings(false);
        return true;
    }
    else if (DSPSignalNotification::match(message))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) message;
        m_deviceCenterFrequency = notif.getCenterFrequency();
        m_basebandSampleRate = notif.getSampleRate();
        ui->deltaFrequency->setValueRange(false, 7, -m_basebandSampleRate/2, m_basebandSampleRate/2);
        ui->deltaFrequencyLabel->setToolTip(tr("Range %1 %L2 Hz").arg(QChar(0xB1)).arg(m_basebandSampleRate/2));
        updateAbsoluteCenterFrequency();
        return true;
    }
    else if (CWMLDemod::MsgText::match(message))
    {
        CWMLDemod::MsgText& report = (CWMLDemod::MsgText&) message;
        textReceived(report.getText());
        return true;
    }
    else if (CWMLDemod::MsgModelStatus::match(message))
    {
        CWMLDemod::MsgModelStatus& report = (CWMLDemod::MsgModelStatus&) message;
        m_modelStatusText = report.getMessage();
        ui->modelStatus->setText(m_modelStatusText);
        ui->modelStatus->setStyleSheet(report.getLoaded() ? "QLabel { color: white }" : "QLabel { color: red }");
        return true;
    }
    else if (CWMLDemod::MsgRecordingStatus::match(message))
    {
        CWMLDemod::MsgRecordingStatus& report = (CWMLDemod::MsgRecordingStatus&) message;
        updateRecordingDisplay(report.getActive(), report.getPath());
        return true;
    }

    return false;
}

void CWMLDemodGUI::handleInputMessages()
{
    Message* message;

    while ((message = getInputMessageQueue()->pop()) != 0)
    {
        if (handleMessage(*message))
        {
            delete message;
        }
    }
}

void CWMLDemodGUI::channelMarkerChangedByCursor()
{
    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void CWMLDemodGUI::channelMarkerHighlightedByCursor()
{
    setHighlighted(m_channelMarker.getHighlighted());
}

void CWMLDemodGUI::on_deltaFrequency_changed(qint64 value)
{
    m_channelMarker.setCenterFrequency(value);
    m_settings.m_inputFrequencyOffset = m_channelMarker.getCenterFrequency();
    updateAbsoluteCenterFrequency();
    applySettings(QStringList({"inputFrequencyOffset"}));
}

void CWMLDemodGUI::on_rfBW_valueChanged(int value)
{
    float bw = value;
    ui->rfBWText->setText(QString("%1 Hz").arg(value));
    m_channelMarker.setBandwidth(bw);
    m_settings.m_rfBandwidth = bw;
    applySettings(QStringList({"rfBandwidth"}));
}

void CWMLDemodGUI::on_modelDir_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        nullptr,
        "Select directory containing model-streaming.onnx",
        m_settings.m_modelDir
    );

    if (!dir.isEmpty())
    {
        m_settings.m_modelDir = dir;
        ui->modelDir->setToolTip(QString("Model directory: %1").arg(m_settings.m_modelDir));
        applySettings(QStringList({"modelDir"}));
    }
}

void CWMLDemodGUI::on_clearText_clicked()
{
    ui->text->clear();
}

void CWMLDemodGUI::on_audioRecord_toggled(bool checked)
{
    m_settings.m_audioRecord = checked;
    applySettings(QStringList({"audioRecord"}));
}

void CWMLDemodGUI::updateRecordingDisplay(bool active, const QString& path)
{
    ui->audioRecord->setStyleSheet(active
        ? "QToolButton { background-color : red; }"
        : "QToolButton { background:rgb(79,79,79); }");

    if (active)
    {
        ui->modelStatus->setText(QString("%1 — REC %2").arg(m_modelStatusText, QFileInfo(path).fileName()));
        ui->audioRecord->setToolTip(QString("Recording to %1").arg(path));
    }
    else
    {
        ui->modelStatus->setText(m_modelStatusText);
        ui->audioRecord->setToolTip(QString("Record decoder input audio to %1 (8 kHz float WAV, new file on retune)")
            .arg(m_settings.m_audioRecordDir));
    }
}

void CWMLDemodGUI::onWidgetRolled(QWidget* widget, bool rollDown)
{
    (void) widget;
    (void) rollDown;

    getRollupContents()->saveState(m_rollupState);
    applySettings(QStringList());
}

void CWMLDemodGUI::onMenuDialogCalled(const QPoint &p)
{
    if (m_contextMenuType == ContextMenuType::ContextMenuChannelSettings)
    {
        BasicChannelSettingsDialog dialog(&m_channelMarker, this);
        dialog.setUseReverseAPI(m_settings.m_useReverseAPI);
        dialog.setReverseAPIAddress(m_settings.m_reverseAPIAddress);
        dialog.setReverseAPIPort(m_settings.m_reverseAPIPort);
        dialog.setReverseAPIDeviceIndex(m_settings.m_reverseAPIDeviceIndex);
        dialog.setReverseAPIChannelIndex(m_settings.m_reverseAPIChannelIndex);
        dialog.setDefaultTitle(m_displayedName);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            dialog.setNumberOfStreams(m_cwmlDemod->getNumberOfDeviceStreams());
            dialog.setStreamIndex(m_settings.m_streamIndex);
        }

        dialog.move(p);
        new DialogPositioner(&dialog, false);
        dialog.exec();

        m_settings.m_rgbColor = m_channelMarker.getColor().rgb();
        m_settings.m_title = m_channelMarker.getTitle();
        m_settings.m_useReverseAPI = dialog.useReverseAPI();
        m_settings.m_reverseAPIAddress = dialog.getReverseAPIAddress();
        m_settings.m_reverseAPIPort = dialog.getReverseAPIPort();
        m_settings.m_reverseAPIDeviceIndex = dialog.getReverseAPIDeviceIndex();
        m_settings.m_reverseAPIChannelIndex = dialog.getReverseAPIChannelIndex();

        setWindowTitle(m_settings.m_title);
        setTitle(m_channelMarker.getTitle());
        setTitleColor(m_settings.m_rgbColor);

        if (m_deviceUISet->m_deviceMIMOEngine)
        {
            m_settings.m_streamIndex = dialog.getSelectedStreamIndex();
            m_channelMarker.clearStreamIndexes();
            m_channelMarker.addStreamIndex(m_settings.m_streamIndex);
            updateIndexLabel();
        }

        applySettings(QStringList({
            "rgbColor",
            "title",
            "useReverseAPI",
            "reverseAPIAddress",
            "reverseAPIPort",
            "reverseAPIDeviceIndex",
            "reverseAPIChannelIndex",
            "streamIndex"
        }));
    }

    resetContextMenuType();
}

CWMLDemodGUI::CWMLDemodGUI(PluginAPI* pluginAPI, DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel, QWidget* parent) :
    ChannelGUI(parent),
    ui(new Ui::CWMLDemodGUI),
    m_pluginAPI(pluginAPI),
    m_deviceUISet(deviceUISet),
    m_channelMarker(this),
    m_deviceCenterFrequency(0),
    m_doApplySettings(true),
    m_basebandSampleRate(0),
    m_tickCount(0),
    m_modelStatusText("Not loaded")
{
    setAttribute(Qt::WA_DeleteOnClose, true);
    m_helpURL = "plugins/channelrx/demodcwml/readme.md";
    RollupContents *rollupContents = getRollupContents();
    ui->setupUi(rollupContents);
    setSizePolicy(rollupContents->sizePolicy());
    rollupContents->arrangeRollups();
    connect(rollupContents, SIGNAL(widgetRolled(QWidget*,bool)), this, SLOT(onWidgetRolled(QWidget*,bool)));
    connect(this, SIGNAL(customContextMenuRequested(const QPoint &)), this, SLOT(onMenuDialogCalled(const QPoint &)));

    m_cwmlDemod = reinterpret_cast<CWMLDemod*>(rxChannel);
    m_cwmlDemod->setMessageQueueToGUI(getInputMessageQueue());

    connect(&MainCore::instance()->getMasterTimer(), SIGNAL(timeout()), this, SLOT(tick())); // 50 ms

    ui->deltaFrequencyLabel->setText(QString("%1f").arg(QChar(0x94, 0x03)));
    ui->deltaFrequency->setColorMapper(ColorMapper(ColorMapper::GrayGold));
    ui->deltaFrequency->setValueRange(false, 7, -9999999, 9999999);
    ui->channelPowerMeter->setColorTheme(LevelMeterSignalDB::ColorGreenAndBlue);

    m_channelMarker.blockSignals(true);
    m_channelMarker.setColor(Qt::yellow);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle("CW Demodulator (ML)");
    m_channelMarker.blockSignals(false);
    m_channelMarker.setVisible(true); // activate signal on the last setting only

    setTitleColor(m_channelMarker.getColor());
    m_settings.setChannelMarker(&m_channelMarker);
    m_settings.setRollupState(&m_rollupState);

    m_deviceUISet->addChannelMarker(&m_channelMarker);

    connect(&m_channelMarker, SIGNAL(changedByCursor()), this, SLOT(channelMarkerChangedByCursor()));
    connect(&m_channelMarker, SIGNAL(highlightedByCursor()), this, SLOT(channelMarkerHighlightedByCursor()));
    connect(getInputMessageQueue(), SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));

    displaySettings();
    makeUIConnections();
    applySettings(QStringList(), true);
    m_resizer.enableChildMouseTracking();
}

CWMLDemodGUI::~CWMLDemodGUI()
{
    delete ui;
}

void CWMLDemodGUI::blockApplySettings(bool block)
{
    m_doApplySettings = !block;
}

void CWMLDemodGUI::applySettings(const QStringList& settingsKeys, bool force)
{
    if (m_doApplySettings)
    {
        CWMLDemod::MsgConfigureCWMLDemod* message = CWMLDemod::MsgConfigureCWMLDemod::create(settingsKeys, m_settings, force);
        m_cwmlDemod->getInputMessageQueue()->push(message);
    }
}

void CWMLDemodGUI::displaySettings()
{
    m_channelMarker.blockSignals(true);
    m_channelMarker.setBandwidth(m_settings.m_rfBandwidth);
    m_channelMarker.setCenterFrequency(m_settings.m_inputFrequencyOffset);
    m_channelMarker.setTitle(m_settings.m_title);
    m_channelMarker.blockSignals(false);
    m_channelMarker.setColor(m_settings.m_rgbColor); // activate signal on the last setting only

    setTitleColor(m_settings.m_rgbColor);
    setWindowTitle(m_channelMarker.getTitle());
    setTitle(m_channelMarker.getTitle());

    blockApplySettings(true);

    ui->deltaFrequency->setValue(m_channelMarker.getCenterFrequency());
    ui->rfBWText->setText(QString("%1 Hz").arg((int) m_settings.m_rfBandwidth));
    ui->rfBW->setValue((int) m_settings.m_rfBandwidth);
    ui->modelDir->setToolTip(QString("Model directory: %1")
        .arg(m_settings.m_modelDir.isEmpty() ? "(built-in default)" : m_settings.m_modelDir));
    ui->audioRecord->setChecked(m_settings.m_audioRecord);
    ui->audioRecord->setToolTip(QString("Record decoder input audio to %1 (8 kHz float WAV, new file on retune)")
        .arg(m_settings.m_audioRecordDir));

    updateIndexLabel();

    getRollupContents()->restoreState(m_rollupState);
    updateAbsoluteCenterFrequency();
    blockApplySettings(false);
}

void CWMLDemodGUI::leaveEvent(QEvent* event)
{
    m_channelMarker.setHighlighted(false);
    ChannelGUI::leaveEvent(event);
}

void CWMLDemodGUI::enterEvent(EnterEventType* event)
{
    m_channelMarker.setHighlighted(true);
    ChannelGUI::enterEvent(event);
}

void CWMLDemodGUI::tick()
{
    double magsqAvg, magsqPeak;
    int nbMagsqSamples;
    m_cwmlDemod->getMagSqLevels(magsqAvg, magsqPeak, nbMagsqSamples);
    double powDbAvg = CalcDb::dbPower(magsqAvg);
    double powDbPeak = CalcDb::dbPower(magsqPeak);
    ui->channelPowerMeter->levelChanged(
            (100.0f + powDbAvg) / 100.0f,
            (100.0f + powDbPeak) / 100.0f,
            nbMagsqSamples);

    if (m_tickCount % 4 == 0) {
        ui->channelPower->setText(QString::number(powDbAvg, 'f', 1));
    }

    m_tickCount++;
}

void CWMLDemodGUI::makeUIConnections()
{
    QObject::connect(ui->deltaFrequency, &ValueDialZ::changed, this, &CWMLDemodGUI::on_deltaFrequency_changed);
    QObject::connect(ui->rfBW, &QSlider::valueChanged, this, &CWMLDemodGUI::on_rfBW_valueChanged);
    QObject::connect(ui->modelDir, &QToolButton::clicked, this, &CWMLDemodGUI::on_modelDir_clicked);
    QObject::connect(ui->clearText, &QToolButton::clicked, this, &CWMLDemodGUI::on_clearText_clicked);
    QObject::connect(ui->audioRecord, &QToolButton::toggled, this, &CWMLDemodGUI::on_audioRecord_toggled);
}

void CWMLDemodGUI::updateAbsoluteCenterFrequency()
{
    setStatusFrequency(m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset);
}
