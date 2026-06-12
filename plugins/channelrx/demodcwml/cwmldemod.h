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

#ifndef INCLUDE_CWMLDEMOD_H
#define INCLUDE_CWMLDEMOD_H

#include <QThread>

#include "dsp/basebandsamplesink.h"
#include "channel/channelapi.h"
#include "util/message.h"

#include "cwmldemodbaseband.h"
#include "cwmldemodsettings.h"

class QThread;
class DeviceAPI;

class CWMLDemod : public BasebandSampleSink, public ChannelAPI {
public:
    class MsgConfigureCWMLDemod : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        const CWMLDemodSettings& getSettings() const { return m_settings; }
        const QStringList& getSettingsKeys() const { return m_settingsKeys; }
        bool getForce() const { return m_force; }

        static MsgConfigureCWMLDemod* create(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force)
        {
            return new MsgConfigureCWMLDemod(settingsKeys, settings, force);
        }

    private:
        CWMLDemodSettings m_settings;
        QStringList m_settingsKeys;
        bool m_force;

        MsgConfigureCWMLDemod(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force) :
            Message(),
            m_settings(settings),
            m_settingsKeys(settingsKeys),
            m_force(force)
        { }
    };

    // Sent from sink when new text is decoded
    class MsgText : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        QString getText() const { return m_text; }

        static MsgText* create(const QString& text)
        {
            return new MsgText(text);
        }

    private:
        QString m_text;

        MsgText(const QString& text) :
            m_text(text)
        {}
    };

    // Sent from sink when the model is (re)loaded or fails
    class MsgModelStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getLoaded() const { return m_loaded; }
        QString getMessage() const { return m_message; }

        static MsgModelStatus* create(bool loaded, const QString& message)
        {
            return new MsgModelStatus(loaded, message);
        }

    private:
        bool m_loaded;
        QString m_message;

        MsgModelStatus(bool loaded, const QString& message) :
            m_loaded(loaded),
            m_message(message)
        {}
    };

    // Sent from Sink when an audio recording file is opened or closed
    class MsgRecordingStatus : public Message {
        MESSAGE_CLASS_DECLARATION

    public:
        bool getActive() const { return m_active; }
        QString getPath() const { return m_path; }

        static MsgRecordingStatus* create(bool active, const QString& path)
        {
            return new MsgRecordingStatus(active, path);
        }

    private:
        bool m_active;
        QString m_path;

        MsgRecordingStatus(bool active, const QString& path) :
            m_active(active),
            m_path(path)
        {}
    };

    CWMLDemod(DeviceAPI *deviceAPI);
    virtual ~CWMLDemod();
    virtual void destroy() { delete this; }
    virtual void setDeviceAPI(DeviceAPI *deviceAPI);
    virtual DeviceAPI *getDeviceAPI() { return m_deviceAPI; }

    using BasebandSampleSink::feed;
    virtual void feed(const SampleVector::const_iterator& begin, const SampleVector::const_iterator& end, bool po);
    virtual void start();
    virtual void stop();
    virtual void pushMessage(Message *msg) { m_inputMessageQueue.push(msg); }
    virtual QString getSinkName() { return objectName(); }

    virtual void getIdentifier(QString& id) { id = objectName(); }
    virtual QString getIdentifier() const { return objectName(); }
    virtual const QString& getURI() const { return getName(); }
    virtual void getTitle(QString& title) { title = m_settings.m_title; }
    virtual qint64 getCenterFrequency() const { return m_settings.m_inputFrequencyOffset; }
    virtual void setCenterFrequency(qint64 frequency);

    virtual QByteArray serialize() const;
    virtual bool deserialize(const QByteArray& data);

    virtual int getNbSinkStreams() const { return 1; }
    virtual int getNbSourceStreams() const { return 0; }
    virtual int getStreamIndex() const { return m_settings.m_streamIndex; }

    virtual qint64 getStreamCenterFrequency(int streamIndex, bool sinkElseSource) const
    {
        (void) streamIndex;
        (void) sinkElseSource;
        return 0;
    }

    double getMagSq() const { return m_basebandSink->getMagSq(); }

    void getMagSqLevels(double& avg, double& peak, int& nbSamples) {
        m_basebandSink->getMagSqLevels(avg, peak, nbSamples);
    }

    uint32_t getNumberOfDeviceStreams() const;

    static const char * const m_channelIdURI;
    static const char * const m_channelId;

private:
    DeviceAPI *m_deviceAPI;
    QThread m_thread;
    CWMLDemodBaseband* m_basebandSink;
    CWMLDemodSettings m_settings;
    int m_basebandSampleRate; //!< stored from device message used when starting baseband sink
    qint64 m_centerFrequency;

    virtual bool handleMessage(const Message& cmd);
    void applySettings(const QStringList& settingsKeys, const CWMLDemodSettings& settings, bool force = false);

private slots:
    void handleIndexInDeviceSetChanged(int index);
};

#endif // INCLUDE_CWMLDEMOD_H
