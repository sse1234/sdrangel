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

#include "cwmldemod.h"

#include <QDebug>
#include <QThread>

#include "dsp/dspcommands.h"
#include "device/deviceapi.h"
#include "settings/serializable.h"
#include "maincore.h"

MESSAGE_CLASS_DEFINITION(CWMLDemod::MsgConfigureCWMLDemod, Message)
MESSAGE_CLASS_DEFINITION(CWMLDemod::MsgText, Message)
MESSAGE_CLASS_DEFINITION(CWMLDemod::MsgModelStatus, Message)

const char * const CWMLDemod::m_channelIdURI = "sdrangel.channel.cwmldemod";
const char * const CWMLDemod::m_channelId = "CWMLDemod";

CWMLDemod::CWMLDemod(DeviceAPI *deviceAPI) :
        ChannelAPI(m_channelIdURI, ChannelAPI::StreamSingleSink),
        m_deviceAPI(deviceAPI),
        m_basebandSampleRate(0),
        m_centerFrequency(0)
{
    setObjectName(m_channelId);

    m_basebandSink = new CWMLDemodBaseband();
    m_basebandSink->setMessageQueueToChannel(getInputMessageQueue());
    m_basebandSink->setChannel(this);
    m_basebandSink->moveToThread(&m_thread);

    applySettings(QStringList(), m_settings, true);

    m_deviceAPI->addChannelSink(this);
    m_deviceAPI->addChannelSinkAPI(this);

    QObject::connect(
        this,
        &ChannelAPI::indexInDeviceSetChanged,
        this,
        &CWMLDemod::handleIndexInDeviceSetChanged
    );
}

CWMLDemod::~CWMLDemod()
{
    qDebug("CWMLDemod::~CWMLDemod");
    m_deviceAPI->removeChannelSinkAPI(this);
    m_deviceAPI->removeChannelSink(this, true);

    if (m_basebandSink->isRunning()) {
        stop();
    }

    delete m_basebandSink;
}

void CWMLDemod::setDeviceAPI(DeviceAPI *deviceAPI)
{
    if (deviceAPI != m_deviceAPI)
    {
        m_deviceAPI->removeChannelSinkAPI(this);
        m_deviceAPI->removeChannelSink(this, false);
        m_deviceAPI = deviceAPI;
        m_deviceAPI->addChannelSink(this);
        m_deviceAPI->addChannelSinkAPI(this);
    }
}

uint32_t CWMLDemod::getNumberOfDeviceStreams() const
{
    return m_deviceAPI->getNbSourceStreams();
}

void CWMLDemod::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool firstOfBurst)
{
    (void) firstOfBurst;
    m_basebandSink->feed(begin, end);
}

void CWMLDemod::start()
{
    qDebug("CWMLDemod::start");

    m_basebandSink->reset();
    m_basebandSink->startWork();
    m_thread.start();

    DSPSignalNotification *dspMsg = new DSPSignalNotification(m_basebandSampleRate, m_centerFrequency);
    m_basebandSink->getInputMessageQueue()->push(dspMsg);

    CWMLDemodBaseband::MsgConfigureCWMLDemodBaseband *msg = CWMLDemodBaseband::MsgConfigureCWMLDemodBaseband::create(QStringList(), m_settings, true);
    m_basebandSink->getInputMessageQueue()->push(msg);
}

void CWMLDemod::stop()
{
    qDebug("CWMLDemod::stop");
    m_basebandSink->stopWork();
    m_thread.quit();
    m_thread.wait();
}

bool CWMLDemod::handleMessage(const Message& cmd)
{
    if (MsgConfigureCWMLDemod::match(cmd))
    {
        MsgConfigureCWMLDemod& cfg = (MsgConfigureCWMLDemod&) cmd;
        qDebug() << "CWMLDemod::handleMessage: MsgConfigureCWMLDemod";
        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());
        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        m_basebandSampleRate = notif.getSampleRate();
        m_centerFrequency = notif.getCenterFrequency();
        // Forward to the sink
        DSPSignalNotification* rep = new DSPSignalNotification(notif); // make a copy
        qDebug() << "CWMLDemod::handleMessage: DSPSignalNotification";
        m_basebandSink->getInputMessageQueue()->push(rep);
        // Forward to GUI if any
        if (m_guiMessageQueue) {
            m_guiMessageQueue->push(new DSPSignalNotification(notif));
        }

        return true;
    }
    else if (CWMLDemod::MsgText::match(cmd))
    {
        // Forward to GUI
        CWMLDemod::MsgText& report = (CWMLDemod::MsgText&)cmd;

        if (getMessageQueueToGUI())
        {
            CWMLDemod::MsgText *msg = new CWMLDemod::MsgText(report);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else if (CWMLDemod::MsgModelStatus::match(cmd))
    {
        // Forward to GUI
        CWMLDemod::MsgModelStatus& report = (CWMLDemod::MsgModelStatus&)cmd;

        if (getMessageQueueToGUI())
        {
            CWMLDemod::MsgModelStatus *msg = new CWMLDemod::MsgModelStatus(report);
            getMessageQueueToGUI()->push(msg);
        }

        return true;
    }
    else
    {
        return false;
    }
}

void CWMLDemod::setCenterFrequency(qint64 frequency)
{
    CWMLDemodSettings settings = m_settings;
    settings.m_inputFrequencyOffset = frequency;
    applySettings(QStringList({"inputFrequencyOffset"}), settings, false);

    if (m_guiMessageQueue) // forward to GUI if any
    {
        MsgConfigureCWMLDemod *msgToGUI = MsgConfigureCWMLDemod::create(QStringList({"inputFrequencyOffset"}), settings, false);
        m_guiMessageQueue->push(msgToGUI);
    }
}

void CWMLDemod::applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force)
{
    qDebug() << "CWMLDemod::applySettings:" << settings.getDebugString(settingsKeys, force);

    if (settingsKeys.contains("streamIndex") && (settings.m_streamIndex != m_settings.m_streamIndex))
    {
        if (m_deviceAPI->getSampleMIMO()) // change of stream is possible for MIMO devices only
        {
            m_deviceAPI->removeChannelSinkAPI(this);
            m_deviceAPI->removeChannelSink(this, m_settings.m_streamIndex);
            m_deviceAPI->addChannelSink(this, settings.m_streamIndex);
            m_deviceAPI->addChannelSinkAPI(this);
            m_settings.m_streamIndex = settings.m_streamIndex; // make sure ChannelAPI::getStreamIndex() is consistent
            emit streamIndexChanged(settings.m_streamIndex);
        }
    }

    CWMLDemodBaseband::MsgConfigureCWMLDemodBaseband *msg = CWMLDemodBaseband::MsgConfigureCWMLDemodBaseband::create(settingsKeys, settings, force);
    m_basebandSink->getInputMessageQueue()->push(msg);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

QByteArray CWMLDemod::serialize() const
{
    return m_settings.serialize();
}

bool CWMLDemod::deserialize(const QByteArray& data)
{
    if (m_settings.deserialize(data))
    {
        MsgConfigureCWMLDemod *msg = MsgConfigureCWMLDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return true;
    }
    else
    {
        m_settings.resetToDefaults();
        MsgConfigureCWMLDemod *msg = MsgConfigureCWMLDemod::create(QStringList(), m_settings, true);
        m_inputMessageQueue.push(msg);
        return false;
    }
}

void CWMLDemod::handleIndexInDeviceSetChanged(int index)
{
    if (index < 0) {
        return;
    }

    QString fifoLabel = QString("%1 [%2:%3]")
        .arg(m_channelId)
        .arg(m_deviceAPI->getDeviceSetIndex())
        .arg(index);
    m_basebandSink->setFifoLabel(fifoLabel);
}
