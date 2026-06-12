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

#include <QDateTime>
#include <QDebug>
#include <QDir>
#include <QFile>
#include <QFileInfo>

#include <complex.h>
#include <cstring>

#include "cw_streaming_decoder.h"

#include "cwmldemod.h"
#include "cwmldemodsink.h"

#ifndef CWML_DEFAULT_MODEL_DIR
#define CWML_DEFAULT_MODEL_DIR ""
#endif

CWMLDemodSink::CWMLDemodSink() :
        m_channel(nullptr),
        m_channelSampleRate(CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE),
        m_channelFrequencyOffset(0),
        m_magsqSum(0.0),
        m_magsqPeak(0.0),
        m_magsqCount(0),
        m_messageQueueToChannel(nullptr)
{
    m_magsq = 0.0;
    m_audioBuffer.reserve(m_audioBlockSize);

    m_bfo.setFreq(CWMLDemodSettings::CWMLDEMOD_PITCH_HZ, CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE);

    applySettings(QStringList(), m_settings, true);
    applyChannelSettings(m_channelSampleRate, m_channelFrequencyOffset, true);
}

CWMLDemodSink::~CWMLDemodSink()
{
    closeWav();
}

// 44-byte canonical header, format 3 (IEEE float32), mono, 8 kHz.
// Sizes are patched in place as the file grows so a crash mid-recording
// still leaves a readable WAV.
void CWMLDemodSink::openWav()
{
    closeWav();

    if (!m_settings.m_audioRecord || m_settings.m_audioRecordDir.isEmpty()) {
        return;
    }

    QDir dir(m_settings.m_audioRecordDir);
    if (!dir.exists() && !dir.mkpath(".")) {
        qWarning() << "CWMLDemodSink::openWav: cannot create" << m_settings.m_audioRecordDir;
        return;
    }

    const qint64 absFrequency = m_deviceCenterFrequency + m_settings.m_inputFrequencyOffset;
    const QString name = QString("cw_%1_%2Hz.wav")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss"))
        .arg(absFrequency);
    const QString path = dir.filePath(name);

    m_wavFile = fopen(QFile::encodeName(path).constData(), "wb");

    if (!m_wavFile) {
        qWarning() << "CWMLDemodSink::openWav: cannot open" << path;
        return;
    }

    const quint32 sampleRate = CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE;
    const quint32 byteRate = sampleRate * 4;
    quint8 header[44];
    memcpy(header, "RIFF\0\0\0\0WAVEfmt ", 16);
    const quint32 fmtSize = 16;
    const quint16 fmtFloat = 3, channels = 1, blockAlign = 4, bitsPerSample = 32;
    memcpy(header + 16, &fmtSize, 4);
    memcpy(header + 20, &fmtFloat, 2);
    memcpy(header + 22, &channels, 2);
    memcpy(header + 24, &sampleRate, 4);
    memcpy(header + 28, &byteRate, 4);
    memcpy(header + 32, &blockAlign, 2);
    memcpy(header + 34, &bitsPerSample, 2);
    memcpy(header + 36, "data\0\0\0\0", 8);
    fwrite(header, 1, sizeof(header), m_wavFile);

    m_wavSampleCount = 0;
    m_wavSamplesSinceHeaderPatch = 0;
    qInfo().noquote() << "CWMLDemodSink: recording to" << path;
}

void CWMLDemodSink::patchWavHeader()
{
    if (!m_wavFile) {
        return;
    }

    const quint32 dataSize = (quint32) (m_wavSampleCount * 4);
    const quint32 riffSize = 36 + dataSize;
    fseek(m_wavFile, 4, SEEK_SET);
    fwrite(&riffSize, 4, 1, m_wavFile);
    fseek(m_wavFile, 40, SEEK_SET);
    fwrite(&dataSize, 4, 1, m_wavFile);
    fseek(m_wavFile, 0, SEEK_END);
    fflush(m_wavFile);
    m_wavSamplesSinceHeaderPatch = 0;
}

void CWMLDemodSink::closeWav()
{
    if (!m_wavFile) {
        return;
    }

    patchWavHeader();
    fclose(m_wavFile);
    m_wavFile = nullptr;

    // Drop empty stubs (e.g. rapid retuning)
    if (m_wavSampleCount == 0) {
        return;
    }

    qInfo() << "CWMLDemodSink: recorded" << m_wavSampleCount << "samples"
            << QString("(%1 s)").arg(m_wavSampleCount / (double) CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE, 0, 'f', 1);
}

void CWMLDemodSink::rotateWav()
{
    if (m_settings.m_audioRecord) {
        openWav();
    } else {
        closeWav();
    }
}

void CWMLDemodSink::writeWavSamples(const float *samples, std::size_t n)
{
    if (!m_wavFile || n == 0) {
        return;
    }

    fwrite(samples, sizeof(float), n, m_wavFile);
    m_wavSampleCount += n;
    m_wavSamplesSinceHeaderPatch += n;

    // Keep the header sizes fresh every ~5 s
    if (m_wavSamplesSinceHeaderPatch >= 5u * CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE) {
        patchWavHeader();
    }
}

void CWMLDemodSink::setDeviceCenterFrequency(qint64 frequency)
{
    if (frequency != m_deviceCenterFrequency)
    {
        m_deviceCenterFrequency = frequency;
        rotateWav();
    }
}

void CWMLDemodSink::feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end)
{
    Complex ci;

    for (SampleVector::const_iterator it = begin; it != end; ++it)
    {
        Complex c(it->real(), it->imag());
        c *= m_nco.nextIQ();

        if (m_interpolatorDistance < 1.0f) // interpolate
        {
            while (!m_interpolator.interpolate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
        else // decimate
        {
            if (m_interpolator.decimate(&m_interpolatorDistanceRemain, c, &ci))
            {
                processOneSample(ci);
                m_interpolatorDistanceRemain += m_interpolatorDistance;
            }
        }
    }
}

void CWMLDemodSink::processOneSample(Complex &ci)
{
    // Calculate average and peak levels for level meter
    double magsqRaw = ci.real()*ci.real() + ci.imag()*ci.imag();
    Real magsq = magsqRaw / (SDR_RX_SCALED*SDR_RX_SCALED);
    m_movingAverage(magsq);
    m_magsq = m_movingAverage.asDouble();
    m_magsqSum += magsq;
    if (magsq > m_magsqPeak) {
        m_magsqPeak = magsq;
    }
    m_magsqCount++;

    ci /= SDR_RX_SCALEF;

    // The model decodes the on/off envelope of ordinary CW receiver audio:
    // shift the (now zero-centered) carrier to an audible pitch and take the
    // real part, exactly like a CW/SSB receiver with a BFO would.
    Complex audio = ci * m_bfo.nextIQ();
    m_audioBuffer.push_back(audio.real());

    if (m_audioBuffer.size() >= m_audioBlockSize) {
        feedPipeline();
    }
}

void CWMLDemodSink::feedPipeline()
{
    writeWavSamples(m_audioBuffer.data(), m_audioBuffer.size());

    if (!m_pipeline)
    {
        m_audioBuffer.clear();
        return;
    }

    std::string text;

    try
    {
        text = m_pipeline->process_samples(m_audioBuffer.data(), m_audioBuffer.size());
    }
    catch (const std::exception& e)
    {
        qWarning() << "CWMLDemodSink::feedPipeline: inference error:" << e.what();
        m_pipeline.reset();
        sendModelStatus(false, QString("Inference error: %1").arg(e.what()));
    }

    m_audioBuffer.clear();

    if (!text.empty() && getMessageQueueToChannel())
    {
        CWMLDemod::MsgText *msg = CWMLDemod::MsgText::create(QString::fromStdString(text));
        getMessageQueueToChannel()->push(msg);
    }
}

void CWMLDemodSink::loadModel()
{
    QString dir = m_settings.m_modelDir.isEmpty() ? QString(CWML_DEFAULT_MODEL_DIR) : m_settings.m_modelDir;
    QString modelPath = dir + "/model-streaming.onnx";
    QString metaPath = dir + "/model-streaming.json";

    m_pipeline.reset();

    if (dir.isEmpty() || !QFileInfo::exists(modelPath) || !QFileInfo::exists(metaPath))
    {
        qWarning() << "CWMLDemodSink::loadModel: model not found in" << dir;
        sendModelStatus(false, QString("No model in %1").arg(dir.isEmpty() ? "(unset)" : dir));
        return;
    }

    try
    {
        const std::string meta = cwml::read_text_file(metaPath.toStdString());
        m_pipeline.reset(new cwml::CWStreamingPipeline(modelPath.toStdString(), meta));
        qDebug() << "CWMLDemodSink::loadModel: loaded" << modelPath
                 << "vocabulary size" << (int) m_pipeline->vocabulary().size();
        sendModelStatus(true, QString("Model loaded (%1 tokens)").arg(m_pipeline->vocabulary().size()));
    }
    catch (const std::exception& e)
    {
        qWarning() << "CWMLDemodSink::loadModel: failed:" << e.what();
        m_pipeline.reset();
        sendModelStatus(false, QString("Load failed: %1").arg(e.what()));
    }
}

void CWMLDemodSink::sendModelStatus(bool loaded, const QString& message)
{
    if (getMessageQueueToChannel()) {
        getMessageQueueToChannel()->push(CWMLDemod::MsgModelStatus::create(loaded, message));
    }
}

void CWMLDemodSink::applyChannelSettings(int channelSampleRate, int channelFrequencyOffset, bool force)
{
    qDebug() << "CWMLDemodSink::applyChannelSettings:"
            << " channelSampleRate: " << channelSampleRate
            << " channelFrequencyOffset: " << channelFrequencyOffset;

    if ((m_channelFrequencyOffset != channelFrequencyOffset) ||
        (m_channelSampleRate != channelSampleRate) || force)
    {
        m_nco.setFreq(-channelFrequencyOffset, channelSampleRate);
    }

    if (m_channelFrequencyOffset != channelFrequencyOffset) {
        rotateWav(); // retuned within the passband: separate recording
    }

    if ((m_channelSampleRate != channelSampleRate) || force)
    {
        m_interpolator.create(16, channelSampleRate, m_settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) channelSampleRate / (Real) CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    m_channelSampleRate = channelSampleRate;
    m_channelFrequencyOffset = channelFrequencyOffset;
}

void CWMLDemodSink::applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force)
{
    qDebug() << "CWMLDemodSink::applySettings:" << settings.getDebugString(settingsKeys, force);

    if ((settingsKeys.contains("rfBandwidth") && (settings.m_rfBandwidth != m_settings.m_rfBandwidth)) || force)
    {
        m_interpolator.create(16, m_channelSampleRate, settings.m_rfBandwidth / 2.2);
        m_interpolatorDistance = (Real) m_channelSampleRate / (Real) CWMLDemodSettings::CWMLDEMOD_CHANNEL_SAMPLE_RATE;
        m_interpolatorDistanceRemain = m_interpolatorDistance;
    }

    bool reloadModel = force || (settingsKeys.contains("modelDir") && (settings.m_modelDir != m_settings.m_modelDir));
    bool recordChanged = force
        || (settingsKeys.contains("audioRecord") && (settings.m_audioRecord != m_settings.m_audioRecord))
        || (settingsKeys.contains("audioRecordDir") && (settings.m_audioRecordDir != m_settings.m_audioRecordDir));

    if (force) {
        m_settings = settings;
    } else {
        m_settings.applySettings(settingsKeys, settings);
    }

    if (reloadModel) {
        loadModel();
    }

    if (recordChanged) {
        rotateWav();
    }
}
