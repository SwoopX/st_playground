#include "de_web_plugin.h"
#include "de_web_plugin_private.h"

// server receive
#define CMD_GET_ALERTS              0x00
// server send
#define CMD_GET_ALERTS_RESPONSE     0x00
#define CMD_ALERTS_NOTIFICATION     0x01
#define CMD_EVENT_NOTIFICATION      0x02

// read flags
#define ALERTS_ALERT (1 << 12)


void DeRestPluginPrivate::handleApplianceAlertClusterIndication(const deCONZ::ApsDataIndication &ind, deCONZ::ZclFrame &zclFrame)
{
    QDataStream stream(zclFrame.payload());
    stream.setByteOrder(QDataStream::LittleEndian);

    if (!(zclFrame.frameControl() & deCONZ::ZclFCDirectionServerToClient))
    {
        DBG_Printf(DBG_INFO_L2, "[APPL] Seems we do not have the right communication direction\n");
        return;
    }
    
    if (zclFrame.isClusterCommand())
    {
        DBG_Printf(DBG_INFO_L2, "[APPL] Command is cluster command\n");
    }
    
    if ((zclFrame.commandId() == CMD_GET_ALERTS_RESPONSE || zclFrame.commandId() == CMD_ALERTS_NOTIFICATION) && zclFrame.isClusterCommand())
    {
        quint8 alertsCount;
        quint16 alertsStructure; // 24 Bit long, but 16 suffice

        stream >> alertsCount;
        stream >> alertsStructure;

        DBG_Printf(DBG_INFO_L2, "[APPL] Alert count: %d\n", alertsCount);
        DBG_Printf(DBG_INFO_L2, "[APPL] Alert Structure: %d\n", alertsStructure);
        
        // Specific to leakSMART water sensor V2
        // Should be more generic
        SensorFingerprint fp;
        fp.endpoint = 0x01;
        fp.deviceId = 0x0302;
        fp.profileId = 0x0104;
        fp.inClusters.push_back(POWER_CONFIGURATION_CLUSTER_ID);
        fp.inClusters.push_back(APPLIANCE_EVENTS_AND_ALERTS_ID);

        Sensor *sensor = getSensorNodeForFingerPrint(ind.srcAddress().ext(), fp, "ZHAWater");
        ResourceItem *item = sensor ? sensor->item(RStateWater) : nullptr;
        
        if (alertsStructure & ALERTS_ALERT)
        {
            DBG_Printf(DBG_INFO_L2, "[APPL] Device currently alerting...\n");
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "[APPL] Device ceased alert...\n");
        }

        if (sensor && item)
        {
            if (alertsStructure & ALERTS_ALERT)
            {
                DBG_Printf(DBG_INFO_L2, "[APPL] Settings sensor state alert to 'true'...\n");
                item->setValue(true);
            }
            else
            {
                DBG_Printf(DBG_INFO_L2, "[APPL] Settings sensor state alert to 'false'...\n");
                item->setValue(false);
            }
            sensor->updateStateTimestamp();
            enqueueEvent(Event(RSensors, RStateWater, sensor->id(), item));
            enqueueEvent(Event(RSensors, RStateLastUpdated, sensor->id()));
            sensor->setNeedSaveDatabase(true);
            queSaveDb(DB_SENSORS, DB_SHORT_SAVE_DELAY);
            updateSensorEtag(&*sensor);
        }
        else
        {
            DBG_Printf(DBG_INFO_L2, "[APPL] Sensor or item not found\n");
        }
    }
    else
    {
        DBG_Printf(DBG_INFO_L2, "[APPL] Seems we haven received the right command\n");
    }
}