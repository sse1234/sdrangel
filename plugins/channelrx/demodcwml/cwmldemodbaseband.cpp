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

#include "dsp/dspcommands.h"
#include "dsp/downchannelizer.h"

#include "cwmldemodbaseband.h"

MESSAGE_CLASS_DEFINITION(CWMLDemodBaseband::MsgConfigureCWMLDemodBaseband, Message)

CWMLDemodBaseband::CWMLDemodBaseband() :
    m_running(false)
{
    qDebug("CWMLDemodBaseband::CWMLDemodBaseband");

    m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(48000));
    m_channelizer = new DownChannelizer(&m_sink);
}

CWMLDemodBaseband::~CWMLDemodBaseband()
{
    m_inputMessageQueue.clear();

    delete m_channelizer;
}

void CWMLDemodBaseband::reset()
{
    QMutexLocker mutexLocker(&m_mutex);
    m_inputMessageQueue.clear();
    m_sampleFifo.reset();
}

void CWMLDemodBaseband::startWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    QObject::connect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &CWMLDemodBaseband::handleData,
        Qt::QueuedConnection
    );
    connect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    m_running = true;
}

void CWMLDemodBaseband::stopWork()
{
    QMutexLocker mutexLocker(&m_mutex);
    disconnect(&m_inputMessageQueue, SIGNAL(messageEnqueued()), this, SLOT(handleInputMessages()));
    QObject::disconnect(
        &m_sampleFifo,
        &SampleSinkFifo::dataReady,
        this,
        &CWMLDemodBaseband::handleData
    );
    m_running = false;
}

void CWMLDemodBaseband::setChannel(ChannelAPI *channel)
{
    m_sink.setChannel(channel);
}

void CWMLDemodBaseband::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    m_sampleFifo.write(begin, end);
}

void CWMLDemodBaseband::handleData()
{
    QMutexLocker mutexLocker(&m_mutex);

    while ((m_sampleFifo.fill() > 0) && (m_inputMessageQueue.size() == 0) && m_channelizer->getBasebandSampleRate())
    {
        SampleVector::iterator part1begin;
        SampleVector::iterator part1end;
        SampleVector::iterator part2begin;
        SampleVector::iterator part2end;

        std::size_t count = m_sampleFifo.readBegin(m_sampleFifo.fill(), &part1begin, &part1end, &part2begin, &part2end);

        // first part of FIFO data
        if (part1begin != part1end) {
            m_channelizer->feed(part1begin, part1end);
        }

        // second part of FIFO data (used when block wraps around)
        if (part2begin != part2end) {
            m_channelizer->feed(part2begin, part2end);
        }

        m_sampleFifo.readCommit((unsigned int) count);
    }
}

void CWMLDemodBaseband::handleInputMessages()
{
    Message* message;

    while ((message = m_inputMessageQueue.pop()) != nullptr)
    {
        if (handleMessage(*message)) {
            delete message;
        }
    }
}

bool CWMLDemodBaseband::handleMessage(const Message& cmd)
{
    if (MsgConfigureCWMLDemodBaseband::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        MsgConfigureCWMLDemodBaseband& cfg = (MsgConfigureCWMLDemodBaseband&) cmd;
        qDebug() << "CWMLDemodBaseband::handleMessage: MsgConfigureCWMLDemodBaseband";

        applySettings(cfg.getSettingsKeys(), cfg.getSettings(), cfg.getForce());

        return true;
    }
    else if (DSPSignalNotification::match(cmd))
    {
        QMutexLocker mutexLocker(&m_mutex);
        DSPSignalNotification& notif = (DSPSignalNotification&) cmd;
        qDebug() << "CWMLDemodBaseband::handleMessage: DSPSignalNotification: basebandSampleRate: " << notif.getSampleRate();
        setBasebandSampleRate(notif.getSampleRate());
        m_sink.setDeviceCenterFrequency(notif.getCenterFrequency());
        // We can run with very slow sample rate (E.g. 4k), but we don't want FIFO getting too small
        m_sampleFifo.setSize(SampleSinkFifo::getSizePolicy(std::max(notif.getSampleRate(), 48000)));

        return true;
    }
    else
    {
        return false;
    }
}

void CWMLDemodBaseband::applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force)
{
    if ((settingsKeys.contains("inputFrequencyOffset") && (settings.m_inputFrequencyOffset != m_settings.m_inputFrequencyOffset)) || force)
    {
        m_channelizer->setChannelization(CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE, settings.m_inputFrequencyOffset);
        m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
    }

    m_sink.applySettings(settingsKeys, settings, force);

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }
}

int CWMLDemodBaseband::getChannelSampleRate() const
{
    return m_channelizer->getChannelSampleRate();
}

void CWMLDemodBaseband::setBasebandSampleRate(int sampleRate)
{
    m_channelizer->setBasebandSampleRate(sampleRate);
    m_sink.applyChannelSettings(m_channelizer->getChannelSampleRate(), m_channelizer->getChannelFrequencyOffset());
}
