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

#include <QColor>
#include <QStandardPaths>
#include <QtGlobal>

#include "util/simpleserializer.h"
#include "settings/serializable.h"
#include "cwmldemodsettings.h"

CWMLDemodSettings::CWMLDemodSettings() :
    m_channelMarker(nullptr),
    m_rollupState(nullptr)
{
    resetToDefaults();
}

void CWMLDemodSettings::resetToDefaults()
{
    m_inputFrequencyOffset = 0;
    m_rfBandwidth = 500.0f;
    m_modelDir = "";

    // CWML_RECORD_DIR in the environment turns recording on from the start —
    // useful for unattended data-collection sessions.
    const QByteArray envDir = qgetenv("CWML_RECORD_DIR");
    m_audioRecord = !envDir.isEmpty();
    m_audioRecordDir = envDir.isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + "/cwml-recordings"
        : QString::fromLocal8Bit(envDir);

    m_rgbColor = QColor(255, 180, 0).rgb();
    m_title = "CW Demodulator (ML)";
    m_streamIndex = 0;
    m_useReverseAPI = false;
    m_reverseAPIAddress = "127.0.0.1";
    m_reverseAPIPort = 8888;
    m_reverseAPIDeviceIndex = 0;
    m_reverseAPIChannelIndex = 0;
    m_workspaceIndex = 0;
    m_hidden = false;
}

QByteArray CWMLDemodSettings::serialize() const
{
    SimpleSerializer s(1);
    s.writeS32(1, m_inputFrequencyOffset);
    s.writeS32(2, m_streamIndex);
    s.writeFloat(3, m_rfBandwidth);
    s.writeString(4, m_modelDir);
    s.writeBool(5, m_audioRecord);
    s.writeString(6, m_audioRecordDir);

    if (m_channelMarker) {
        s.writeBlob(11, m_channelMarker->serialize());
    }

    s.writeU32(12, m_rgbColor);
    s.writeString(13, m_title);
    s.writeBool(14, m_useReverseAPI);
    s.writeString(15, m_reverseAPIAddress);
    s.writeU32(16, m_reverseAPIPort);
    s.writeU32(17, m_reverseAPIDeviceIndex);
    s.writeU32(18, m_reverseAPIChannelIndex);

    if (m_rollupState) {
        s.writeBlob(27, m_rollupState->serialize());
    }

    s.writeS32(28, m_workspaceIndex);
    s.writeBlob(29, m_geometryBytes);
    s.writeBool(30, m_hidden);

    return s.final();
}

bool CWMLDemodSettings::deserialize(const QByteArray& data)
{
    SimpleDeserializer d(data);

    if (!d.isValid())
    {
        resetToDefaults();
        return false;
    }

    if (d.getVersion() == 1)
    {
        QByteArray bytetmp;
        uint32_t utmp;

        d.readS32(1, &m_inputFrequencyOffset, 0);
        d.readS32(2, &m_streamIndex, 0);
        d.readFloat(3, &m_rfBandwidth, 500.0f);
        d.readString(4, &m_modelDir, "");
        d.readBool(5, &m_audioRecord, m_audioRecord);
        d.readString(6, &m_audioRecordDir, m_audioRecordDir);

        if (m_channelMarker)
        {
            d.readBlob(11, &bytetmp);
            m_channelMarker->deserialize(bytetmp);
        }

        d.readU32(12, &m_rgbColor, QColor(255, 180, 0).rgb());
        d.readString(13, &m_title, "CW Demodulator (ML)");
        d.readBool(14, &m_useReverseAPI, false);
        d.readString(15, &m_reverseAPIAddress, "127.0.0.1");
        d.readU32(16, &utmp, 0);

        if ((utmp > 1023) && (utmp < 65535)) {
            m_reverseAPIPort = utmp;
        } else {
            m_reverseAPIPort = 8888;
        }

        d.readU32(17, &utmp, 0);
        m_reverseAPIDeviceIndex = utmp > 99 ? 99 : utmp;
        d.readU32(18, &utmp, 0);
        m_reverseAPIChannelIndex = utmp > 99 ? 99 : utmp;

        if (m_rollupState)
        {
            d.readBlob(27, &bytetmp);
            m_rollupState->deserialize(bytetmp);
        }

        d.readS32(28, &m_workspaceIndex, 0);
        d.readBlob(29, &m_geometryBytes);
        d.readBool(30, &m_hidden, false);

        return true;
    }
    else
    {
        resetToDefaults();
        return false;
    }
}

void CWMLDemodSettings::applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings)
{
    if (settingsKeys.contains("inputFrequencyOffset")) {
        m_inputFrequencyOffset = settings.m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth")) {
        m_rfBandwidth = settings.m_rfBandwidth;
    }
    if (settingsKeys.contains("modelDir")) {
        m_modelDir = settings.m_modelDir;
    }
    if (settingsKeys.contains("audioRecord")) {
        m_audioRecord = settings.m_audioRecord;
    }
    if (settingsKeys.contains("audioRecordDir")) {
        m_audioRecordDir = settings.m_audioRecordDir;
    }
    if (settingsKeys.contains("rgbColor")) {
        m_rgbColor = settings.m_rgbColor;
    }
    if (settingsKeys.contains("title")) {
        m_title = settings.m_title;
    }
    if (settingsKeys.contains("streamIndex")) {
        m_streamIndex = settings.m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI")) {
        m_useReverseAPI = settings.m_useReverseAPI;
    }
    if (settingsKeys.contains("reverseAPIAddress")) {
        m_reverseAPIAddress = settings.m_reverseAPIAddress;
    }
    if (settingsKeys.contains("reverseAPIPort")) {
        m_reverseAPIPort = settings.m_reverseAPIPort;
    }
    if (settingsKeys.contains("reverseAPIDeviceIndex")) {
        m_reverseAPIDeviceIndex = settings.m_reverseAPIDeviceIndex;
    }
    if (settingsKeys.contains("reverseAPIChannelIndex")) {
        m_reverseAPIChannelIndex = settings.m_reverseAPIChannelIndex;
    }
    if (settingsKeys.contains("workspaceIndex")) {
        m_workspaceIndex = settings.m_workspaceIndex;
    }
    if (settingsKeys.contains("geometryBytes")) {
        m_geometryBytes = settings.m_geometryBytes;
    }
    if (settingsKeys.contains("hidden")) {
        m_hidden = settings.m_hidden;
    }
}

QString CWMLDemodSettings::getDebugString(const QStringList& settingsKeys, bool force) const
{
    std::ostringstream ostr;

    if (settingsKeys.contains("inputFrequencyOffset") || force) {
        ostr << " m_inputFrequencyOffset: " << m_inputFrequencyOffset;
    }
    if (settingsKeys.contains("rfBandwidth") || force) {
        ostr << " m_rfBandwidth: " << m_rfBandwidth;
    }
    if (settingsKeys.contains("modelDir") || force) {
        ostr << " m_modelDir: " << m_modelDir.toStdString();
    }
    if (settingsKeys.contains("audioRecord") || force) {
        ostr << " m_audioRecord: " << m_audioRecord;
    }
    if (settingsKeys.contains("audioRecordDir") || force) {
        ostr << " m_audioRecordDir: " << m_audioRecordDir.toStdString();
    }
    if (settingsKeys.contains("rgbColor") || force) {
        ostr << " m_rgbColor: " << m_rgbColor;
    }
    if (settingsKeys.contains("title") || force) {
        ostr << " m_title: " << m_title.toStdString();
    }
    if (settingsKeys.contains("streamIndex") || force) {
        ostr << " m_streamIndex: " << m_streamIndex;
    }
    if (settingsKeys.contains("useReverseAPI") || force) {
        ostr << " m_useReverseAPI: " << m_useReverseAPI;
    }
    if (settingsKeys.contains("workspaceIndex") || force) {
        ostr << " m_workspaceIndex: " << m_workspaceIndex;
    }
    if (settingsKeys.contains("hidden") || force) {
        ostr << " m_hidden: " << m_hidden;
    }

    return QString(ostr.str().c_str());
}
