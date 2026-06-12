///////////////////////////////////////////////////////////////////////////////////
// Copyright (C) 2026 morse-pro contributors                                     //
//                                                                               //
// ML-based CW demodulator using the morse-pro streaming Conformer/CTC decoder.  //
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

#ifndef INCLUDE_CWMLDEMODSETTINGS_H
#define INCLUDE_CWMLDEMODSETTINGS_H

#include <QByteArray>
#include <QString>

#include "dsp/dsptypes.h"

class Serializable;

struct CWMLDemodSettings
{
    qint32 m_inputFrequencyOffset;
    Real m_rfBandwidth;
    QString m_modelDir;     //!< directory containing model-streaming.onnx/.json (empty = built-in default)
    bool m_audioRecord;     //!< record decoder input audio (8 kHz float WAV, one file per tuning)
    QString m_audioRecordDir;

    quint32 m_rgbColor;
    QString m_title;
    Serializable *m_channelMarker;
    int m_streamIndex; //!< MIMO channel. Not relevant when connected to SI (single Rx).
    bool m_useReverseAPI;
    QString m_reverseAPIAddress;
    uint16_t m_reverseAPIPort;
    uint16_t m_reverseAPIDeviceIndex;
    uint16_t m_reverseAPIChannelIndex;

    Serializable *m_rollupState;
    int m_workspaceIndex;
    QByteArray m_geometryBytes;
    bool m_hidden;

    // Must match the feature extractor / training sample rate of the model
    static const int CWMLDEMOD_CHANNEL_SAMPLE_RATE = 8000;
    // Audio pitch the carrier is shifted to before the envelope extractor
    static const int CWMLDEMOD_PITCH_HZ = 600;

    CWMLDemodSettings();
    void resetToDefaults();
    void setChannelMarker(Serializable *channelMarker) { m_channelMarker = channelMarker; }
    void setRollupState(Serializable *rollupState) { m_rollupState = rollupState; }
    QByteArray serialize() const;
    bool deserialize(const QByteArray& data);
    void applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings);
    QString getDebugString(const QStringList& settingsKeys, bool force=false) const;
};

#endif /* INCLUDE_CWMLDEMODSETTINGS_H */
