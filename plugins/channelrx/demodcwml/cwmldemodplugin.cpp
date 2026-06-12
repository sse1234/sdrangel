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

#include <QtPlugin>
#include "plugin/pluginapi.h"

#ifndef SERVER_MODE
#include "cwmldemodgui.h"
#endif
#include "cwmldemod.h"
#include "cwmldemodwebapiadapter.h"
#include "cwmldemodplugin.h"

const PluginDescriptor CWMLDemodPlugin::m_pluginDescriptor = {
    CWMLDemod::m_channelId,
    QStringLiteral("CW Demodulator (ML)"),
    QStringLiteral("0.1.0"),
    QStringLiteral("(c) morse-pro contributors"),
    QStringLiteral("https://github.com/scp93ch/morse-pro"),
    true,
    QStringLiteral("https://github.com/scp93ch/morse-pro")
};

CWMLDemodPlugin::CWMLDemodPlugin(QObject* parent) :
    QObject(parent),
    m_pluginAPI(0)
{
}

const PluginDescriptor& CWMLDemodPlugin::getPluginDescriptor() const
{
    return m_pluginDescriptor;
}

void CWMLDemodPlugin::initPlugin(PluginAPI* pluginAPI)
{
    m_pluginAPI = pluginAPI;

    m_pluginAPI->registerRxChannel(CWMLDemod::m_channelIdURI, CWMLDemod::m_channelId, this);
}

void CWMLDemodPlugin::createRxChannel(DeviceAPI *deviceAPI, BasebandSampleSink **bs, ChannelAPI **cs) const
{
    if (bs || cs)
    {
        CWMLDemod *instance = new CWMLDemod(deviceAPI);

        if (bs) {
            *bs = instance;
        }

        if (cs) {
            *cs = instance;
        }
    }
}

#ifdef SERVER_MODE
ChannelGUI* CWMLDemodPlugin::createRxChannelGUI(
        DeviceUISet *deviceUISet,
        BasebandSampleSink *rxChannel) const
{
    (void) deviceUISet;
    (void) rxChannel;
    return 0;
}
#else
ChannelGUI* CWMLDemodPlugin::createRxChannelGUI(DeviceUISet *deviceUISet, BasebandSampleSink *rxChannel) const
{
    return CWMLDemodGUI::create(m_pluginAPI, deviceUISet, rxChannel);
}
#endif

ChannelWebAPIAdapter* CWMLDemodPlugin::createChannelWebAPIAdapter() const
{
    return new CWMLDemodWebAPIAdapter();
}
