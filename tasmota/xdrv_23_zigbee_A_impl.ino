/*
  xdrv_23_zigbee.ino - zigbee support for Tasmota

  Copyright (C) 2020  Theo Arends and Stephan Hadinger

  This program is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#ifdef USE_ZIGBEE

#define XDRV_23                    23

const char kZbCommands[] PROGMEM = D_PRFX_ZB "|"    // prefix
#ifdef USE_ZIGBEE_ZNP
  D_CMND_ZIGBEEZNPSEND "|" D_CMND_ZIGBEEZNPRECEIVE "|"
#endif // USE_ZIGBEE_ZNP
#ifdef USE_ZIGBEE_EZSP
  D_CMND_ZIGBEE_EZSP_SEND "|" D_CMND_ZIGBEE_EZSP_RECEIVE "|" D_CMND_ZIGBEE_EZSP_LISTEN "|"
#endif // USE_ZIGBEE_EZSP
  D_CMND_ZIGBEE_PERMITJOIN "|"
  D_CMND_ZIGBEE_STATUS "|" D_CMND_ZIGBEE_RESET "|" D_CMND_ZIGBEE_SEND "|" D_CMND_ZIGBEE_PROBE "|"
  D_CMND_ZIGBEE_FORGET "|" D_CMND_ZIGBEE_SAVE "|" D_CMND_ZIGBEE_NAME "|"
  D_CMND_ZIGBEE_BIND "|" D_CMND_ZIGBEE_UNBIND "|" D_CMND_ZIGBEE_PING "|" D_CMND_ZIGBEE_MODELID "|"
  D_CMND_ZIGBEE_LIGHT "|" D_CMND_ZIGBEE_OCCUPANCY "|" 
  D_CMND_ZIGBEE_RESTORE "|" D_CMND_ZIGBEE_BIND_STATE "|" D_CMND_ZIGBEE_MAP "|"
  D_CMND_ZIGBEE_CONFIG "|" D_CMND_ZIGBEE_DATA
  ;

void (* const ZigbeeCommand[])(void) PROGMEM = {
#ifdef USE_ZIGBEE_ZNP
  &CmndZbZNPSend, &CmndZbZNPReceive,
#endif // USE_ZIGBEE_ZNP
#ifdef USE_ZIGBEE_EZSP
  &CmndZbEZSPSend, &CmndZbEZSPReceive, &CmndZbEZSPListen,
#endif // USE_ZIGBEE_EZSP
  &CmndZbPermitJoin,
  &CmndZbStatus, &CmndZbReset, &CmndZbSend, &CmndZbProbe,
  &CmndZbForget, &CmndZbSave, &CmndZbName,
  &CmndZbBind, &CmndZbUnbind, &CmndZbPing, &CmndZbModelId,
  &CmndZbLight, &CmndZbOccupancy, 
  &CmndZbRestore, &CmndZbBindState, &CmndZbMap,
  &CmndZbConfig, CmndZbData,
  };

/********************************************************************************************/

// Initialize internal structures
void ZigbeeInit(void)
{
// #pragma GCC diagnostic push
// #pragma GCC diagnostic ignored "-Winvalid-offsetof"
// Serial.printf(">>> offset %d %d %d\n", Z_offset(Z_Data_Light, dimmer), Z_offset(Z_Data_Light, x), Z_offset(Z_Data_Thermo, temperature));
// #pragma GCC diagnostic pop
  // Check if settings in Flash are set
  if (PinUsed(GPIO_ZIGBEE_RX) && PinUsed(GPIO_ZIGBEE_TX)) {
    if (0 == Settings.zb_channel) {
      AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Randomizing Zigbee parameters, please check with 'ZbConfig'"));
      uint64_t mac64 = 0;     // stuff mac address into 64 bits
      WiFi.macAddress((uint8_t*) &mac64);
      uint32_t esp_id = ESP_getChipId();
#ifdef ESP8266
      uint32_t flash_id = ESP.getFlashChipId();
#else  // ESP32
      uint32_t flash_id = 0;
#endif  // ESP8266 or ESP32

      uint16_t  pan_id = (mac64 & 0x3FFF);
      if (0x0000 == pan_id) { pan_id = 0x0001; }    // avoid extreme values
      if (0x3FFF == pan_id) { pan_id = 0x3FFE; }    // avoid extreme values
      Settings.zb_pan_id = pan_id;

      Settings.zb_ext_panid = 0xCCCCCCCC00000000L | (mac64 & 0x00000000FFFFFFFFL);
      Settings.zb_precfgkey_l = (mac64 << 32) | (esp_id << 16) | flash_id;
      Settings.zb_precfgkey_h = (mac64 << 32) | (esp_id << 16) | flash_id;
      Settings.zb_channel = USE_ZIGBEE_CHANNEL;
      Settings.zb_txradio_dbm = USE_ZIGBEE_TXRADIO_DBM;
    }

    if (Settings.zb_txradio_dbm < 0) {
      Settings.zb_txradio_dbm = -Settings.zb_txradio_dbm;
#ifdef USE_ZIGBEE_EZSP
      EZ_reset_config = true;         // force reconfigure of EZSP
#endif
      SettingsSave(2);
    }
  }

  // update commands with the current settings
#ifdef USE_ZIGBEE_ZNP
  ZNP_UpdateConfig(Settings.zb_channel, Settings.zb_pan_id, Settings.zb_ext_panid, Settings.zb_precfgkey_l, Settings.zb_precfgkey_h);
#endif
#ifdef USE_ZIGBEE_EZSP
  EZ_UpdateConfig(Settings.zb_channel, Settings.zb_pan_id, Settings.zb_ext_panid, Settings.zb_precfgkey_l, Settings.zb_precfgkey_h, Settings.zb_txradio_dbm);
#endif

  ZigbeeInitSerial();
}

/*********************************************************************************************\
 * Commands
\*********************************************************************************************/

#ifdef USE_ZIGBEE_ZNP
// Do a factory reset of the CC2530
const unsigned char ZIGBEE_FACTORY_RESET[] PROGMEM =
  { Z_SREQ | Z_SAPI, SAPI_WRITE_CONFIGURATION, CONF_STARTUP_OPTION, 0x01 /* len */, 0x01 /* STARTOPT_CLEAR_CONFIG */};
//"2605030101";  // Z_SREQ | Z_SAPI, SAPI_WRITE_CONFIGURATION, CONF_STARTUP_OPTION, 0x01 len, 0x01 STARTOPT_CLEAR_CONFIG
#endif // USE_ZIGBEE_ZNP

void CmndZbReset(void) {
  if (ZigbeeSerial) {
    switch (XdrvMailbox.payload) {
    case 1:
#ifdef USE_ZIGBEE_ZNP
      ZigbeeZNPSend(ZIGBEE_FACTORY_RESET, sizeof(ZIGBEE_FACTORY_RESET));
#endif // USE_ZIGBEE_ZNP
      eraseZigbeeDevices();
    case 2:   // fall through
      Settings.zb_txradio_dbm = - abs(Settings.zb_txradio_dbm);
      TasmotaGlobal.restart_flag = 2;
#ifdef USE_ZIGBEE_ZNP
      ResponseCmndChar_P(PSTR(D_JSON_ZIGBEE_CC2530 " " D_JSON_RESET_AND_RESTARTING));
#endif // USE_ZIGBEE_ZNP
#ifdef USE_ZIGBEE_EZSP
      ResponseCmndChar_P(PSTR(D_JSON_ZIGBEE_EZSP " " D_JSON_RESET_AND_RESTARTING));
#endif // USE_ZIGBEE_EZSP
      break;
    default:
      ResponseCmndChar_P(PSTR("1 or 2 to reset"));
    }
  }
}

/********************************************************************************************/
//
// High-level function
// Send a command specified as an HEX string for the workload.
// The target endpoint is computed if zero, i.e. sent to the first known endpoint of the device.
//
// Inputs:
// - shortaddr: 16-bits short address, or 0x0000 if group address
// - groupaddr: 16-bits group address, or 0x0000 if unicast using shortaddr
// - endpoint:  8-bits target endpoint (source is always 0x01), if 0x00, it will be guessed from ZbStatus information (basically the first endpoint of the device)
// - clusterSpecific: boolean, is the message general cluster or cluster specific, used to create the FC byte of ZCL
// - clusterIf: 16-bits cluster number
// - param:     pointer to HEX string for payload, should not be nullptr
// Returns: None
//
void zigbeeZCLSendStr(uint16_t shortaddr, uint16_t groupaddr, uint8_t endpoint, bool clusterSpecific, uint16_t manuf,
                       uint16_t cluster, uint8_t cmd, const char *param) {
  size_t size = param ? strlen(param) : 0;
  SBuffer buf((size+2)/2);    // actual bytes buffer for data

  if (param) {
    while (*param) {
      uint8_t code = parseHex_P(&param, 2);
      buf.add8(code);
    }
  }

  if ((0 == endpoint) && (BAD_SHORTADDR != shortaddr)) {
    // endpoint is not specified, let's try to find it from shortAddr, unless it's a group address
    endpoint = zigbee_devices.findFirstEndpoint(shortaddr);
    //AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZbSend: guessing endpoint 0x%02X"), endpoint);
  }
  AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZbSend: shortaddr 0x%04X, groupaddr 0x%04X, cluster 0x%04X, endpoint 0x%02X, cmd 0x%02X, data %s"),
    shortaddr, groupaddr, cluster, endpoint, cmd, param);

  if ((0 == endpoint) && (BAD_SHORTADDR != shortaddr)) {     // endpoint null is ok for group address
    AddLog_P2(LOG_LEVEL_INFO, PSTR("ZbSend: unspecified endpoint"));
    return;
  }

  // everything is good, we can send the command

  uint8_t seq = zigbee_devices.getNextSeqNumber(shortaddr);
  ZigbeeZCLSend_Raw(ZigbeeZCLSendMessage({
    shortaddr,
    groupaddr,
    cluster /*cluster*/,
    endpoint,
    cmd,
    manuf,  /* manuf */
    clusterSpecific /* not cluster specific */,
    true /* response */,
    false /* discover route */,
    seq,  /* zcl transaction id */
    buf.getBuffer(), buf.len()
  }));
  // now set the timer, if any, to read back the state later
  if (clusterSpecific) {
#ifndef USE_ZIGBEE_NO_READ_ATTRIBUTES   // read back attribute value unless it is disabled
    sendHueUpdate(shortaddr, groupaddr, cluster, endpoint);
#endif
  }
}

// Special encoding for multiplier:
// multiplier == 0: ignore
// multiplier == 1: ignore
// multiplier > 0: divide by the multiplier
// multiplier < 0: multiply by the -multiplier (positive)
void ZbApplyMultiplier(double &val_d, int8_t multiplier) {
  if ((0 != multiplier) && (1 != multiplier)) {
    if (multiplier > 0) {         // inverse of decoding
      val_d = val_d / multiplier;
    } else {
      val_d = val_d * (-multiplier);
    }
  }
}

//
// Write Tuya-Moes attribute
//
bool ZbTuyaWrite(SBuffer & buf, const Z_attribute & attr, uint8_t transid) {
  double val_d = attr.getFloat();
  const char * val_str = attr.getStr();

  if (attr.key_is_str) { return false; }    // couldn't find attr if so skip
  if (attr.isNum() && (1 != attr.attr_multiplier)) {
    ZbApplyMultiplier(val_d, attr.attr_multiplier);
  }
  buf.add8(0);                      // status
  buf.add8(transid);                // transid
  buf.add16(attr.key.id.attr_id);   // dp
  buf.add8(0);                      // fn
  int32_t res = encodeSingleAttribute(buf, val_d, val_str, attr.attr_type);
  if (res < 0) {
    AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Error for Tuya attribute type %04X/%04X '0x%02X'"), attr.key.id.cluster, attr.key.id.attr_id, attr.attr_type);
    return false;
  }
  return true;
}

//
// Send Attribute Write, apply mutlipliers before
//
bool ZbAppendWriteBuf(SBuffer & buf, const Z_attribute & attr, bool prepend_status_ok) {
  double val_d = attr.getFloat();
  const char * val_str = attr.getStr();

  if (attr.key_is_str) { return false; }    // couldn't find attr if so skip
  if (attr.isNum() && (1 != attr.attr_multiplier)) {
    ZbApplyMultiplier(val_d, attr.attr_multiplier);
  }

  // push the value in the buffer
  buf.add16(attr.key.id.attr_id);        // prepend with attribute identifier
  if (prepend_status_ok) {
    buf.add8(Z_SUCCESS);  // status OK = 0x00
  }
  buf.add8(attr.attr_type);     // prepend with attribute type
  int32_t res = encodeSingleAttribute(buf, val_d, val_str, attr.attr_type);
  if (res < 0) {
    // remove the attribute type we just added
    // buf.setLen(buf.len() - (operation == ZCL_READ_ATTRIBUTES_RESPONSE ? 4 : 3));
    AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Unsupported attribute type %04X/%04X '0x%02X'"), attr.key.id.cluster, attr.key.id.attr_id, attr.attr_type);
    return false;
  }
  return true;
}

//
// Parse "Report", "Write", "Response" or "Config" attribute
// Operation is one of: ZCL_REPORT_ATTRIBUTES (0x0A), ZCL_WRITE_ATTRIBUTES (0x02) or ZCL_READ_ATTRIBUTES_RESPONSE (0x01)
//
void ZbSendReportWrite(class JsonParserToken val_pubwrite, class ZigbeeZCLSendMessage & packet) {
  SBuffer buf(200);       // buffer to store the binary output of attibutes

  if (nullptr == XdrvMailbox.command) {
    XdrvMailbox.command = (char*) "";             // prevent a crash when calling ReponseCmndChar and there was no previous command
  }

  // iterate on keys
  for (auto key : val_pubwrite.getObject()) {
    JsonParserToken value = key.getValue();

    Z_attribute attr;
    attr.setKeyName(key.getStr());
    if (Z_parseAttributeKey(attr)) {
      // Buffer ready, do some sanity checks

      // all attributes must use the same cluster
      if (0xFFFF == packet.cluster) {
        packet.cluster = attr.key.id.cluster;       // set the cluster for this packet
      } else if (packet.cluster != attr.key.id.cluster) {
        ResponseCmndChar_P(PSTR("No more than one cluster id per command"));
        return;
      }

    } else {
      if (attr.key_is_str) {
        Response_P(PSTR("{\"%s\":\"%s'%s'\"}"), XdrvMailbox.command, PSTR("Unknown attribute "), key);
        return;
      }
      if (Zunk == attr.attr_type) {
        Response_P(PSTR("{\"%s\":\"%s'%s'\"}"), XdrvMailbox.command, PSTR("Unknown attribute type for attribute "), key);
        return;
      }
    }

    // copy value from input to attribute, in numerical or string format
    if (value.isStr()) {
      attr.setStr(value.getStr());
    } else if (value.isNum()) {
      attr.setFloat(value.getFloat());
    }

    double   val_d = 0;             // I try to avoid `double` but this type capture both float and (u)int32_t without prevision loss
    const char* val_str = "";       // variant as string
    ////////////////////////////////////////////////////////////////////////////////
    // Split encoding depending on message
    if (packet.cmd != ZCL_CONFIGURE_REPORTING) {
      if ((packet.cluster == 0XEF00) && (packet.cmd == ZCL_WRITE_ATTRIBUTES)) {
        // special case of Tuya / Moes attributes
        if (buf.len() > 0) {
          AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "Only 1 attribute allowed for Tuya"));
          return;
        }
        packet.clusterSpecific = true;
        packet.cmd = 0x00;
        if (!ZbTuyaWrite(buf, attr, zigbee_devices.getNextSeqNumber(packet.shortaddr))) {
          return;   // error
        }
      } else if (!ZbAppendWriteBuf(buf, attr, packet.cmd == ZCL_READ_ATTRIBUTES_RESPONSE)) { // general case
        return;   // error
      }
    } else {
      // ////////////////////////////////////////////////////////////////////////////////
      // ZCL_CONFIGURE_REPORTING
      if (!value.isObject()) {
        ResponseCmndChar_P(PSTR("Config requires JSON objects"));
        return;
      }
      JsonParserObject attr_config = value.getObject();
      bool attr_direction = false;

      uint32_t dir = attr_config.getUInt(PSTR("DirectionReceived"), 0);
      if (dir) { attr_direction = true; }

      // read MinInterval and MaxInterval, default to 0xFFFF if not specified
      uint16_t attr_min_interval = attr_config.getUInt(PSTR("MinInterval"), 0xFFFF);
      uint16_t attr_max_interval = attr_config.getUInt(PSTR("MaxInterval"), 0xFFFF);

      // read ReportableChange
      JsonParserToken val_attr_rc = attr_config[PSTR("ReportableChange")];
      if (val_attr_rc) {
        val_d = val_attr_rc.getFloat();
        val_str = val_attr_rc.getStr();
        ZbApplyMultiplier(val_d, attr.attr_multiplier);
      }

      // read TimeoutPeriod
      uint16_t attr_timeout = attr_config.getUInt(PSTR("TimeoutPeriod"), 0x0000);

      bool attr_discrete = Z_isDiscreteDataType(attr.attr_type);

      // all fields are gathered, output the butes into the buffer, ZCL 2.5.7.1
      // common bytes
      buf.add8(attr_direction ? 0x01 : 0x00);
      buf.add16(attr.key.id.attr_id);
      if (attr_direction) {
        buf.add16(attr_timeout);
      } else {
        buf.add8(attr.attr_type);
        buf.add16(attr_min_interval);
        buf.add16(attr_max_interval);
        if (!attr_discrete) {
          int32_t res = encodeSingleAttribute(buf, val_d, val_str, attr.attr_type);
          if (res < 0) {
            Response_P(PSTR("{\"%s\":\"%s'%s' 0x%02X\"}"), XdrvMailbox.command, PSTR("Unsupported attribute type "), key, attr.attr_type);
            return;
          }
        }
      }
    }
  }

  // did we have any attribute?
  if (0 == buf.len()) {
    ResponseCmndChar_P(PSTR("No attribute in list"));
    return;
  }

  // all good, send the packet
  packet.transacId = zigbee_devices.getNextSeqNumber(packet.shortaddr);
  packet.msg = buf.getBuffer();
  packet.len = buf.len();
  ZigbeeZCLSend_Raw(packet);
  ResponseCmndDone();
}

// Parse the "Send" attribute and send the command
void ZbSendSend(class JsonParserToken val_cmd, uint16_t device, uint16_t groupaddr, uint16_t cluster, uint8_t endpoint, uint16_t manuf) {
  uint8_t  cmd = 0;
  String   cmd_str = "";          // the actual low-level command, either specified or computed
  const char *cmd_s = "";                 // pointer to payload string
  bool     clusterSpecific = true;

  static char delim[] = ", ";     // delimiters for parameters
  // probe the type of the argument
  // If JSON object, it's high level commands
  // If String, it's a low level command
  if (val_cmd.isObject()) {
    // we have a high-level command
    JsonParserObject cmd_obj = val_cmd.getObject();
    int32_t cmd_size = cmd_obj.size();
    if (cmd_size > 1) {
      Response_P(PSTR("Only 1 command allowed (%d)"), cmd_size);
      return;
    } else if (1 == cmd_size) {
      // We have exactly 1 command, parse it
      JsonParserKey key = cmd_obj.getFirstElement();
      JsonParserToken value = key.getValue();
      uint32_t x = 0, y = 0, z = 0;
      uint16_t cmd_var;
      uint16_t local_cluster_id;

      const __FlashStringHelper* tasmota_cmd = zigbeeFindCommand(key.getStr(), &local_cluster_id, &cmd_var);
      if (tasmota_cmd) {
        cmd_str = tasmota_cmd;
      } else {
        Response_P(PSTR("Unrecognized zigbee command: %s"), key.getStr());
        return;
      }
      // check cluster
      if (0xFFFF == cluster) {
        cluster = local_cluster_id;
      } else if (cluster != local_cluster_id) {
        ResponseCmndChar_P(PSTR("No more than one cluster id per command"));
        return;
      }

      // parse the JSON value, depending on its type fill in x,y,z
      if (value.isNum()) {
        x = value.getUInt();    // automatic conversion to 0/1
      // if (value.is<bool>()) {
      // //   x = value.as<bool>() ? 1 : 0;
      // } else if
      // } else if (value.is<unsigned int>()) {
      //   x = value.as<unsigned int>();
      } else {
        // if non-bool or non-int, trying char*
        const char *s_const = value.getStr(nullptr);
        // const char *s_const = value.as<const char*>();
        if (s_const != nullptr) {
          char s[strlen(s_const)+1];
          strcpy(s, s_const);
          if ((nullptr != s) && (0x00 != *s)) {     // ignore any null or empty string, could represent 'null' json value
            char *sval = strtok(s, delim);
            if (sval) {
              x = ZigbeeAliasOrNumber(sval);
              sval = strtok(nullptr, delim);
              if (sval) {
                y = ZigbeeAliasOrNumber(sval);
                sval = strtok(nullptr, delim);
                if (sval) {
                  z = ZigbeeAliasOrNumber(sval);
                }
              }
            }
          }
        }
      }

      //AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZbSend: command_template = %s"), cmd_str.c_str());
      if (0xFF == cmd_var) {      // if command number is a variable, replace it with x
        cmd = x;
        x = y;                  // and shift other variables
        y = z;
      } else {
        cmd = cmd_var;          // or simply copy the cmd number
      }
      cmd_str = zigbeeCmdAddParams(cmd_str.c_str(), x, y, z);   // fill in parameters
      //AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZbSend: command_final    = %s"), cmd_str.c_str());
      cmd_s = cmd_str.c_str();
    } else {
      // we have zero command, pass through until last error for missing command
    }
  } else if (val_cmd.isStr()) {
    // low-level command
    // Now parse the string to extract cluster, command, and payload
    // Parse 'cmd' in the form "AAAA_BB/CCCCCCCC" or "AAAA!BB/CCCCCCCC"
    // where AA is the cluster number, BBBB the command number, CCCC... the payload
    // First delimiter is '_' for a global command, or '!' for a cluster specific command
    const char * data = val_cmd.getStr();
    uint16_t local_cluster_id = parseHex(&data, 4);

    // check cluster
    if (0xFFFF == cluster) {
      cluster = local_cluster_id;
    } else if (cluster != local_cluster_id) {
      ResponseCmndChar_P(PSTR("No more than one cluster id per command"));
      return;
    }

    // delimiter
    if (('_' == *data) || ('!' == *data)) {
      if ('_' == *data) { clusterSpecific = false; }
      data++;
    } else {
      ResponseCmndChar_P(PSTR("Wrong delimiter for payload"));
      return;
    }
    // parse cmd number
    cmd = parseHex(&data, 2);

    // move to end of payload
    // delimiter is optional
    if ('/' == *data) { data++; }   // skip delimiter

    cmd_s = data;
  } else {
    // we have an unsupported command type, just ignore it and fallback to missing command
  }

  AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZigbeeZCLSend device: 0x%04X, group: 0x%04X, endpoint:%d, cluster:0x%04X, cmd:0x%02X, send:\"%s\""),
            device, groupaddr, endpoint, cluster, cmd, cmd_s);
  zigbeeZCLSendStr(device, groupaddr, endpoint, clusterSpecific, manuf, cluster, cmd, cmd_s);
  ResponseCmndDone();
}


// Parse the "Send" attribute and send the command
void ZbSendRead(JsonParserToken val_attr, ZigbeeZCLSendMessage & packet) {
  // ZbSend {"Device":"0xF289","Cluster":0,"Endpoint":3,"Read":5}
  // ZbSend {"Device":"0xF289","Cluster":"0x0000","Endpoint":"0x0003","Read":"0x0005"}
  // ZbSend {"Device":"0xF289","Cluster":0,"Endpoint":3,"Read":[5,6,7,4]}
  // ZbSend {"Device":"0xF289","Endpoint":3,"Read":{"ModelId":true}}
  // ZbSend {"Device":"0xF289","Read":{"ModelId":true}}

  // ZbSend {"Device":"0xF289","ReadConig":{"Power":true}}
  // ZbSend {"Device":"0xF289","Cluster":6,"Endpoint":3,"ReadConfig":0}

  // params
  size_t   attrs_len = 0;
  uint8_t* attrs = nullptr;       // empty string is valid
  size_t   attr_item_len = 2;     // how many bytes per attribute, standard for "Read"
  size_t   attr_item_offset = 0;  // how many bytes do we offset to store attribute
  if (ZCL_READ_REPORTING_CONFIGURATION == packet.cmd) {
    attr_item_len = 3;
    attr_item_offset = 1;
  }

  if (val_attr.isArray()) {
    // value is an array []
    JsonParserArray attr_arr = val_attr.getArray();
    attrs_len = attr_arr.size() * attr_item_len;
    attrs = (uint8_t*) calloc(attrs_len, 1);

    uint32_t i = 0;
    for (auto value : attr_arr) {
      uint16_t val = value.getUInt();
      i += attr_item_offset;
      attrs[i++] = val & 0xFF;
      attrs[i++] = val >> 8;
      i += attr_item_len - 2 - attr_item_offset;    // normally 0
    }
  } else if (val_attr.isObject()) {
    // value is an object {}
    JsonParserObject attr_obj = val_attr.getObject();
    attrs_len = attr_obj.size() * attr_item_len;
    attrs = (uint8_t*) calloc(attrs_len, 1);
    uint32_t actual_attr_len = 0;

    // iterate on keys
    for (auto key : attr_obj) {
      JsonParserToken value = key.getValue();

      bool found = false;
      // scan attributes to find by name, and retrieve type
      for (uint32_t i = 0; i < ARRAY_SIZE(Z_PostProcess); i++) {
        const Z_AttributeConverter *converter = &Z_PostProcess[i];
        bool match = false;
        uint16_t local_attr_id = pgm_read_word(&converter->attribute);
        uint16_t local_cluster_id = CxToCluster(pgm_read_byte(&converter->cluster_short));
        // uint8_t  local_type_id = pgm_read_byte(&converter->type);

        if ((pgm_read_word(&converter->name_offset)) && (0 == strcasecmp_P(key.getStr(), Z_strings + pgm_read_word(&converter->name_offset)))) {
          // match name
          // check if there is a conflict with cluster
          // TODO
          if (!(value.getBool()) && attr_item_offset) {
            // If value is false (non-default) then set direction to 1 (for ReadConfig)
            attrs[actual_attr_len] = 0x01;
          }
          actual_attr_len += attr_item_offset;
          attrs[actual_attr_len++] = local_attr_id & 0xFF;
          attrs[actual_attr_len++] = local_attr_id >> 8;
          actual_attr_len += attr_item_len - 2 - attr_item_offset;    // normally 0
          found = true;
          // check cluster
          if (0xFFFF == packet.cluster) {
            packet.cluster = local_cluster_id;
          } else if (packet.cluster != local_cluster_id) {
            ResponseCmndChar_P(PSTR("No more than one cluster id per command"));
            if (attrs) { free(attrs); }
            return;
          }
          break;    // found, exit loop
        }
      }
      if (!found) {
        AddLog_P2(LOG_LEVEL_INFO, PSTR("ZIG: Unknown attribute name (ignored): %s"), key);
      }
    }

    attrs_len = actual_attr_len;
  } else {
    // value is a literal
    if (0xFFFF != packet.cluster) {
      uint16_t val = val_attr.getUInt();
      attrs_len = attr_item_len;
      attrs = (uint8_t*) calloc(attrs_len, 1);
      attrs[0 + attr_item_offset] = val & 0xFF;    // little endian
      attrs[1 + attr_item_offset] = val >> 8;
    }
  }

  if (attrs_len > 0) {
    // all good, send the packet
    packet.transacId = zigbee_devices.getNextSeqNumber(packet.shortaddr);
    packet.msg = attrs;
    packet.len = attrs_len;
    ZigbeeZCLSend_Raw(packet);
    ResponseCmndDone();
  } else {
    ResponseCmndChar_P(PSTR("Missing parameters"));
  }

  if (attrs) { free(attrs); }
}

//
// Command `ZbSend`
//
// Examples:
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"0006/0000":0}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"Power":0}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"AqaraRotate":0}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"AqaraRotate":12.5}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"006/0000%39":12.5}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"AnalogInApplicationType":1000000}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"TimeZone":-1000000}}
// ZbSend {"Device":"0x0000","Endpoint":1,"Write":{"Manufacturer":"Tasmota","ModelId":"Tasmota Z2T Router"}}
void CmndZbSend(void) {
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":1} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":"3"} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":"0xFF"} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":null} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":false} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":true} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":"true"} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"ShutterClose":null} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Power":1} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Color":"1,2"} }
  // ZbSend { "device":"0x1234", "endpoint":"0x03", "send":{"Color":"0x1122,0xFFEE"} }
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  JsonParser parser(XdrvMailbox.data);
  JsonParserObject root = parser.getRootObject();
  if (!root) { ResponseCmndChar_P(PSTR(D_JSON_INVALID_JSON)); return; }

  // params
  uint16_t device = BAD_SHORTADDR;    // BAD_SHORTADDR is broadcast, so considered invalid
  uint16_t groupaddr = 0x0000;        // group address valid only if device == BAD_SHORTADDR
  uint16_t cluster = 0xFFFF;          // no default
  uint8_t  endpoint = 0x00;           // 0x00 is invalid for the dst endpoint
  uint16_t manuf = 0x0000;            // Manuf Id in ZCL frame


  // parse "Device" and "Group"
  JsonParserToken val_device = root[PSTR(D_CMND_ZIGBEE_DEVICE)];
  if (val_device) {
    device = zigbee_devices.parseDeviceFromName(val_device.getStr(), true).shortaddr;
    if (BAD_SHORTADDR == device) { ResponseCmndChar_P(PSTR("Invalid parameter")); return; }
  }
  if (BAD_SHORTADDR == device) {     // if not found, check if we have a group
    JsonParserToken val_group = root[PSTR(D_CMND_ZIGBEE_GROUP)];
    if (val_group) {
      groupaddr = val_group.getUInt();
    } else {                  // no device nor group
      ResponseCmndChar_P(PSTR("Unknown device"));
      return;
    }
  }
  // from here, either device has a device shortaddr, or if BAD_SHORTADDR then use group address
  // Note: groupaddr == 0 is valid

  // read other parameters
  cluster = root.getUInt(PSTR(D_CMND_ZIGBEE_CLUSTER), cluster);
  endpoint = root.getUInt(PSTR(D_CMND_ZIGBEE_ENDPOINT), endpoint);
  manuf = root.getUInt(PSTR(D_CMND_ZIGBEE_MANUF), manuf);

  // infer endpoint
  if (BAD_SHORTADDR == device) {
    endpoint = 0xFF;                  // endpoint not used for group addresses, so use a dummy broadcast endpoint
  } else if (0 == endpoint) {         // if it was not already specified, try to guess it
    endpoint = zigbee_devices.findFirstEndpoint(device);
    AddLog_P2(LOG_LEVEL_DEBUG, PSTR("ZIG: guessing endpoint %d"), endpoint);
  }
  if (0 == endpoint) {                // after this, if it is still zero, then it's an error
      ResponseCmndChar_P(PSTR("Missing endpoint"));
      return;
  }
  // from here endpoint is valid and non-zero
  // cluster may be already specified or 0xFFFF

  JsonParserToken val_cmd = root[PSTR(D_CMND_ZIGBEE_SEND)];
  JsonParserToken val_read = root[PSTR(D_CMND_ZIGBEE_READ)];
  JsonParserToken val_write = root[PSTR(D_CMND_ZIGBEE_WRITE)];
  JsonParserToken val_publish = root[PSTR(D_CMND_ZIGBEE_REPORT)];
  JsonParserToken val_response = root[PSTR(D_CMND_ZIGBEE_RESPONSE)];
  JsonParserToken val_read_config = root[PSTR(D_CMND_ZIGBEE_READ_CONFIG)];
  JsonParserToken val_config = root[PSTR(D_CMND_ZIGBEE_CONFIG)];
  uint32_t multi_cmd = ((bool)val_cmd) + ((bool)val_read) + ((bool)val_write) + ((bool)val_publish)
                     + ((bool)val_response) + ((bool)val_read_config) + ((bool)val_config);
  if (multi_cmd > 1) {
    ResponseCmndChar_P(PSTR("Can only have one of: 'Send', 'Read', 'Write', 'Report', 'Reponse', 'ReadConfig' or 'Config'"));
    return;
  }
  // from here we have one and only one command

  // collate information in a ready to send packet
  ZigbeeZCLSendMessage packet({
    device,
    groupaddr,
    cluster /*cluster*/,
    endpoint,
    ZCL_READ_ATTRIBUTES,
    manuf,  /* manuf */
    false /* not cluster specific */,
    false /* no response */,
    false /* discover route */,
    0,  /* zcl transaction id */
    nullptr, 0
  });

  if (val_cmd) {
    // "Send":{...commands...}
    // we accept either a string or a JSON object
    ZbSendSend(val_cmd, device, groupaddr, cluster, endpoint, manuf);
  } else if (val_read) {
    // "Read":{...attributes...}, "Read":attribute or "Read":[...attributes...]
    // we accept eitehr a number, a string, an array of numbers/strings, or a JSON object
    packet.cmd = ZCL_READ_ATTRIBUTES;
    ZbSendRead(val_read, packet);
  } else if (val_write) {
    // only KSON object
    if (!val_write.isObject()) {
      ResponseCmndChar_P(PSTR("Missing parameters"));
      return;
    }
    // "Write":{...attributes...}
    packet.cmd = ZCL_WRITE_ATTRIBUTES;
    ZbSendReportWrite(val_write, packet);
  } else if (val_publish) {
    // "Publish":{...attributes...}
    // only KSON object
    if (!val_publish.isObject()) {
      ResponseCmndChar_P(PSTR("Missing parameters"));
      return;
    }
    packet.cmd = ZCL_REPORT_ATTRIBUTES;
    ZbSendReportWrite(val_publish, packet);
  } else if (val_response) {
    // "Report":{...attributes...}
    // only KSON object
    if (!val_response.isObject()) {
      ResponseCmndChar_P(PSTR("Missing parameters"));
      return;
    }
    packet.cmd = ZCL_READ_ATTRIBUTES_RESPONSE;
    ZbSendReportWrite(val_response, packet);
  } else if (val_read_config) {
    // "ReadConfg":{...attributes...}, "ReadConfg":attribute or "ReadConfg":[...attributes...]
    // we accept eitehr a number, a string, an array of numbers/strings, or a JSON object
    packet.cmd = ZCL_READ_REPORTING_CONFIGURATION;
    ZbSendRead(val_read_config, packet);
  } else if (val_config) {
    // "Config":{...attributes...}
    // only JSON object
    if (!val_config.isObject()) {
      ResponseCmndChar_P(PSTR("Missing parameters"));
      return;
    }
    packet.cmd = ZCL_CONFIGURE_REPORTING;
    ZbSendReportWrite(val_config, packet);
  } else {
    Response_P(PSTR("Missing zigbee 'Send', 'Write', 'Report' or 'Response'"));
    return;
  }
}

//
// Command `ZbBind`
//
void ZbBindUnbind(bool unbind) {    // false = bind, true = unbind
  // ZbBind {"Device":"<device>", "Endpoint":<endpoint>, "Cluster":<cluster>, "ToDevice":"<to_device>", "ToEndpoint":<to_endpoint>, "ToGroup":<to_group> }
  // ZbUnbind {"Device":"<device>", "Endpoint":<endpoint>, "Cluster":<cluster>, "ToDevice":"<to_device>", "ToEndpoint":<to_endpoint>, "ToGroup":<to_group> }

  // local endpoint is always 1, IEEE addresses are calculated
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  JsonParser parser(XdrvMailbox.data);
  JsonParserObject root = parser.getRootObject();
  if (!root) { ResponseCmndChar_P(PSTR(D_JSON_INVALID_JSON)); return; }

  // params
  uint16_t srcDevice = BAD_SHORTADDR;      // BAD_SHORTADDR is broadcast, so considered invalid
  uint16_t dstDevice = BAD_SHORTADDR;      // BAD_SHORTADDR is broadcast, so considered invalid
  uint64_t dstLongAddr = 0;
  uint8_t  endpoint = 0x00;         // 0x00 is invalid for the src endpoint
  uint8_t  toendpoint = 0x01;       // default dest endpoint to 0x01
  uint16_t toGroup = 0x0000;        // group address
  uint16_t cluster  = 0;            // cluster 0 is default
  uint32_t group = 0xFFFFFFFF;      // 16 bits values, otherwise 0xFFFFFFFF is unspecified

  // Information about source device: "Device", "Endpoint", "Cluster"
  //  - the source endpoint must have a known IEEE address
  const Z_Device & src_device = zigbee_devices.parseDeviceFromName(root.getStr(PSTR(D_CMND_ZIGBEE_DEVICE), nullptr), true);
  if (!src_device.valid()) { ResponseCmndChar_P(PSTR("Unknown source device")); return; }
  // check if IEEE address is known
  uint64_t srcLongAddr = src_device.longaddr;
  if (0 == srcLongAddr) { ResponseCmndChar_P(PSTR("Unknown source IEEE address")); return; }
  // look for source endpoint
  endpoint = root.getUInt(PSTR(D_CMND_ZIGBEE_ENDPOINT), endpoint);
  if (0 == endpoint) { endpoint = zigbee_devices.findFirstEndpoint(src_device.shortaddr); }
  // look for source cluster
  JsonParserToken val_cluster = root[PSTR(D_CMND_ZIGBEE_CLUSTER)];
  if (val_cluster) {
    cluster = val_cluster.getUInt(cluster);   // first convert as number
    if (0 == cluster) {
      zigbeeFindAttributeByName(val_cluster.getStr(), &cluster, nullptr, nullptr);
    }
  }

  // Or Group Address - we don't need a dstEndpoint in this case
  JsonParserToken to_group = root[PSTR("ToGroup")];
  if (to_group) { toGroup = to_group.getUInt(toGroup); }

  // Either Device address
  // In this case the following parameters are mandatory
  //  - "ToDevice" and the device must have a known IEEE address
  //  - "ToEndpoint"
  JsonParserToken dst_device = root[PSTR("ToDevice")];

  // If no target is specified, we default to coordinator 0x0000
  if ((!to_group) && (!dst_device)) {
    dstDevice = 0x0000;
    dstLongAddr = localIEEEAddr;
    toendpoint = 1;
  }

  if (dst_device) {
    const Z_Device & dstDevice = zigbee_devices.parseDeviceFromName(dst_device.getStr(nullptr), true);
    if (!dstDevice.valid()) { ResponseCmndChar_P(PSTR("Unknown dest device")); return; }
    dstLongAddr = dstDevice.longaddr;
  }

  if (!to_group) {
    if (0 == dstLongAddr) { ResponseCmndChar_P(PSTR("Unknown dest IEEE address")); return; }
    toendpoint = root.getUInt(PSTR("ToEndpoint"), toendpoint);
  }

  // make sure we don't have conflicting parameters
  if (to_group && dst_device) { ResponseCmndChar_P(PSTR("Cannot have both \"ToDevice\" and \"ToGroup\"")); return; }

#ifdef USE_ZIGBEE_ZNP
  SBuffer buf(34);
  buf.add8(Z_SREQ | Z_ZDO);
  if (unbind) {
    buf.add8(ZDO_UNBIND_REQ);
  } else {
    buf.add8(ZDO_BIND_REQ);
  }
  buf.add16(srcDevice);
  buf.add64(srcLongAddr);
  buf.add8(endpoint);
  buf.add16(cluster);
  if (!to_group) {
    buf.add8(Z_Addr_IEEEAddress);         // DstAddrMode - 0x03 = ADDRESS_64_BIT
    buf.add64(dstLongAddr);
    buf.add8(toendpoint);
  } else {
    buf.add8(Z_Addr_Group);               // DstAddrMode - 0x01 = GROUP_ADDRESS
    buf.add16(toGroup);
  }

  ZigbeeZNPSend(buf.getBuffer(), buf.len());
#endif // USE_ZIGBEE_ZNP

#ifdef USE_ZIGBEE_EZSP
  SBuffer buf(24);

  // ZDO message payload (see Zigbee spec 2.4.3.2.2)
  buf.add64(srcLongAddr);
  buf.add8(endpoint);
  buf.add16(cluster);
  if (!to_group) {
    buf.add8(Z_Addr_IEEEAddress);         // DstAddrMode - 0x03 = ADDRESS_64_BIT
    buf.add64(dstLongAddr);
    buf.add8(toendpoint);
  } else {
    buf.add8(Z_Addr_Group);               // DstAddrMode - 0x01 = GROUP_ADDRESS
    buf.add16(toGroup);
  }

  EZ_SendZDO(srcDevice, unbind ? ZDO_UNBIND_REQ : ZDO_BIND_REQ, buf.buf(), buf.len());
#endif // USE_ZIGBEE_EZSP

  ResponseCmndDone();
}

//
// Command ZbBind
//
void CmndZbBind(void) {
  ZbBindUnbind(false);
}

//
// Command ZbBind
//
void CmndZbUnbind(void) {
  ZbBindUnbind(true);
}

void CmndZbBindState_or_Map(uint16_t zdo_cmd) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  uint16_t shortaddr = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true).shortaddr;
  if (BAD_SHORTADDR == shortaddr) { ResponseCmndChar_P(PSTR("Unknown device")); return; }
  uint8_t index = XdrvMailbox.index - 1;   // change default 1 to 0

#ifdef USE_ZIGBEE_ZNP
  SBuffer buf(10);
  buf.add8(Z_SREQ | Z_ZDO);             // 25
  buf.add8(zdo_cmd);                    // 33
  buf.add16(shortaddr);                 // shortaddr
  buf.add8(index);                      // StartIndex = 0

  ZigbeeZNPSend(buf.getBuffer(), buf.len());
#endif // USE_ZIGBEE_ZNP


#ifdef USE_ZIGBEE_EZSP
  // ZDO message payload (see Zigbee spec 2.4.3.3.4)
  uint8_t buf[] = { index };           // index = 0

  EZ_SendZDO(shortaddr, zdo_cmd, buf, sizeof(buf));
#endif // USE_ZIGBEE_EZSP

  ResponseCmndDone();
}

//
// Command `ZbBindState`
// `ZbBindState<x>` as index if it does not fit. If default, `1` starts at the beginning
//
void CmndZbBindState(void) {
#ifdef USE_ZIGBEE_ZNP
  CmndZbBindState_or_Map(ZDO_MGMT_BIND_REQ);
#endif // USE_ZIGBEE_ZNP
#ifdef USE_ZIGBEE_EZSP
  CmndZbBindState_or_Map(ZDO_Mgmt_Bind_req);
#endif // USE_ZIGBEE_EZSP
}

//
// Command `ZbMap`
// `ZbMap<x>` as index if it does not fit. If default, `1` starts at the beginning
//
void CmndZbMap(void) {
#ifdef USE_ZIGBEE_ZNP
  CmndZbBindState_or_Map(ZDO_MGMT_LQI_REQ);
#endif // USE_ZIGBEE_ZNP
#ifdef USE_ZIGBEE_EZSP
  CmndZbBindState_or_Map(ZDO_Mgmt_Lqi_req);
#endif // USE_ZIGBEE_EZSP
}

// Probe a specific device to get its endpoints and supported clusters
void CmndZbProbe(void) {
  CmndZbProbeOrPing(true);
}

//
// Common code for `ZbProbe` and `ZbPing`
//
void CmndZbProbeOrPing(boolean probe) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  uint16_t shortaddr = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true).shortaddr;
  if (BAD_SHORTADDR == shortaddr) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

  // everything is good, we can send the command
  Z_SendIEEEAddrReq(shortaddr);
  if (probe) {
    Z_SendActiveEpReq(shortaddr);
  }
  ResponseCmndDone();
}

// Ping a device, actually a simplified version of ZbProbe
void CmndZbPing(void) {
  CmndZbProbeOrPing(false);
}

//
// Command `ZbName`
// Specify, read or erase a Friendly Name
//
void CmndZbName(void) {
  // Syntax is:
  //  ZbName <device_id>,<friendlyname>  - assign a friendly name
  //  ZbName <device_id>                 - display the current friendly name
  //  ZbName <device_id>,                - remove friendly name
  //
  // Where <device_id> can be: short_addr, long_addr, device_index, friendly_name

  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  // check if parameters contain a comma ','
  char *p;
  char *str = strtok_r(XdrvMailbox.data, ",", &p);

  // parse first part, <device_id>
  Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, false);  // it's the only case where we create a new device
  if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

  if (p == nullptr) {
    const char * friendlyName = device.friendlyName;
    Response_P(PSTR("{\"0x%04X\":{\"" D_JSON_ZIGBEE_NAME "\":\"%s\"}}"), device.shortaddr, friendlyName ? friendlyName : "");
  } else {
    if (strlen(p) > 32) { p[32] = 0x00; }     // truncate to 32 chars max
    device.setFriendlyName(p);
    Response_P(PSTR("{\"0x%04X\":{\"" D_JSON_ZIGBEE_NAME "\":\"%s\"}}"), device.shortaddr, p);
  }
}

//
// Command `ZbName`
// Specify, read or erase a ModelId, only for debug purposes
//
void CmndZbModelId(void) {
  // Syntax is:
  //  ZbName <device_id>,<friendlyname>  - assign a friendly name
  //  ZbName <device_id>                 - display the current friendly name
  //  ZbName <device_id>,                - remove friendly name
  //
  // Where <device_id> can be: short_addr, long_addr, device_index, friendly_name

  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  // check if parameters contain a comma ','
  char *p;
  char *str = strtok_r(XdrvMailbox.data, ",", &p);

  // parse first part, <device_id>
  Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);  // in case of short_addr, it must be already registered
  if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

  if (p == nullptr) {
    const char * modelId = device.modelId;
    Response_P(PSTR("{\"0x%04X\":{\"" D_JSON_ZIGBEE_MODELID "\":\"%s\"}}"), device.shortaddr, modelId ? modelId : "");
  } else {
    device.setModelId(p);
    Response_P(PSTR("{\"0x%04X\":{\"" D_JSON_ZIGBEE_MODELID "\":\"%s\"}}"), device.shortaddr, p);
  }
}

//
// Command `ZbLight`
// Specify, read or erase a Light type for Hue/Alexa integration
void CmndZbLight(void) {
  // Syntax is:
  //  ZbLight <device_id>,<x>            - assign a bulb type 0-5
  //  ZbLight <device_id>                - display the current bulb type and status
  //
  // Where <device_id> can be: short_addr, long_addr, device_index, friendly_name

  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  // check if parameters contain a comma ','
  char *p;
  char *str = strtok_r(XdrvMailbox.data, ", ", &p);

  // parse first part, <device_id>
  Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);  // in case of short_addr, it must be already registered
  if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

  if (p) {
    int8_t bulbtype = strtol(p, nullptr, 10);
    if (bulbtype > 5)  { bulbtype = 5; }
    if (bulbtype < -1) { bulbtype = -1; }
    device.setLightChannels(bulbtype);
  }
  String dump = zigbee_devices.dumpLightState(device);
  Response_P(PSTR("{\"" D_PRFX_ZB D_CMND_ZIGBEE_LIGHT "\":%s}"), dump.c_str());

  MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_STAT, PSTR(D_PRFX_ZB D_CMND_ZIGBEE_LIGHT));
  ResponseCmndDone();
}
//
// Command `ZbOccupancy`
// Specify, read or erase the Occupancy detector configuration
void CmndZbOccupancy(void) {
  // Syntax is:
  //  ZbOccupancy <device_id>,<x>            - set the occupancy time-out
  //  ZbOccupancy <device_id>                - display the configuration
  //
  // List of occupancy time-outs:
  // 0xF = default (90 s)
  // 0x0 = no time-out
  // 0x1 = 15 s
  // 0x2 = 30 s
  // 0x3 = 45 s
  // 0x4 = 60 s
  // 0x5 = 75 s
  // 0x6 = 90 s -- default
  // 0x7 = 105 s
  // 0x8 = 120 s
  // Where <device_id> can be: short_addr, long_addr, device_index, friendly_name

  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  // check if parameters contain a comma ','
  char *p;
  char *str = strtok_r(XdrvMailbox.data, ", ", &p);

  // parse first part, <device_id>
  Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);  // in case of short_addr, it must be already registered
  if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }
  
  int8_t occupancy_time = -1;
  if (p) {
    Z_Data_PIR & pir = (Z_Data_PIR&) device.data.getByType(Z_Data_Type::Z_PIR);
    occupancy_time = strtol(p, nullptr, 10);
    pir.setTimeoutSeconds(occupancy_time);
    zigbee_devices.dirty();
  } else {
    const Z_Data_PIR & pir_found = (const Z_Data_PIR&) device.data.find(Z_Data_Type::Z_PIR);
    if (&pir_found != nullptr) {
      occupancy_time = pir_found.getTimeoutSeconds();
    }
  }
  Response_P(PSTR("{\"" D_PRFX_ZB D_CMND_ZIGBEE_OCCUPANCY "\":%d}"), occupancy_time);

  MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_STAT, PSTR(D_PRFX_ZB D_CMND_ZIGBEE_LIGHT));
  ResponseCmndDone();
}

//
// Command `ZbForget`
// Remove an old Zigbee device from the list of known devices, use ZigbeeStatus to know all registered devices
//
void CmndZbForget(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);  // in case of short_addr, it must be already registered
  if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

  // everything is good, we can send the command
  if (zigbee_devices.removeDevice(device.shortaddr)) {
    ResponseCmndDone();
  } else {
    ResponseCmndChar_P(PSTR("Unknown device"));
  }
}

//
// Command `ZbSave`
// Save Zigbee information to flash
//
void CmndZbSave(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  saveZigbeeDevices();
  ResponseCmndDone();
}


// Restore a device configuration previously exported via `ZbStatus2``
// Format:
// Either the entire `ZbStatus3` export, or an array or just the device configuration.
// If array, if can contain multiple devices
//   ZbRestore {"ZbStatus3":[{"Device":"0x5ADF","Name":"Petite_Lampe","IEEEAddr":"0x90FD9FFFFE03B051","ModelId":"TRADFRI bulb E27 WS opal 980lm","Manufacturer":"IKEA of Sweden","Endpoints":["0x01","0xF2"]}]}
//   ZbRestore [{"Device":"0x5ADF","Name":"Petite_Lampe","IEEEAddr":"0x90FD9FFFFE03B051","ModelId":"TRADFRI bulb E27 WS opal 980lm","Manufacturer":"IKEA of Sweden","Endpoints":["0x01","0xF2"]}]
//   ZbRestore {"Device":"0x5ADF","Name":"Petite_Lampe","IEEEAddr":"0x90FD9FFFFE03B051","ModelId":"TRADFRI bulb E27 WS opal 980lm","Manufacturer":"IKEA of Sweden","Endpoints":["0x01","0xF2"]}
void CmndZbRestore(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  JsonParser parser(XdrvMailbox.data);
  JsonParserToken root = parser.getRoot();

  if (!parser || !(root.isObject() || root.isArray())) { ResponseCmndChar_P(PSTR(D_JSON_INVALID_JSON)); return; }

  // Check is root contains `ZbStatus<x>` key, if so change the root
  JsonParserToken zbstatus = root.getObject().findStartsWith(PSTR("ZbStatus"));
  if (zbstatus) {
    root = zbstatus;
  }

  // check if the root is an array
  if (root.isArray()) {
    JsonParserArray arr = JsonParserArray(root);
    for (const auto elt : arr) {
      // call restore on each item
      if (elt.isObject()) {
        int32_t res = zigbee_devices.deviceRestore(JsonParserObject(elt));
        if (res < 0) {
          ResponseCmndChar_P(PSTR("Restore failed"));
          return;
        }
      }
    }
  } else if (root.isObject()) {
    int32_t res = zigbee_devices.deviceRestore(JsonParserObject(root));
    if (res < 0) {
      ResponseCmndChar_P(PSTR("Restore failed"));
      return;
    }
    // call restore on a single object
  } else {
    ResponseCmndChar_P(PSTR("Missing parameters"));
    return;
  }
  ResponseCmndDone();
}

//
// Command `ZbPermitJoin`
// Allow or Deny pairing of new Zigbee devices
//
void CmndZbPermitJoin(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  uint32_t payload = XdrvMailbox.payload;
  uint8_t  duration = 60;               // default 60s

  if (payload <= 0) {
    duration = 0;
  }

// ZNP Version
#ifdef USE_ZIGBEE_ZNP
  if (99 == payload) {
    duration = 0xFF;                    // unlimited time
  }

  uint16_t dstAddr = 0xFFFC;            // default addr

  SBuffer buf(34);
  buf.add8(Z_SREQ | Z_ZDO);             // 25
  buf.add8(ZDO_MGMT_PERMIT_JOIN_REQ);   // 36
  buf.add8(0x0F);                       // AddrMode
  buf.add16(0xFFFC);                    // DstAddr
  buf.add8(duration);
  buf.add8(0x00);                       // TCSignificance

  ZigbeeZNPSend(buf.getBuffer(), buf.len());

#endif // USE_ZIGBEE_ZNP

// EZSP VERSION
#ifdef USE_ZIGBEE_EZSP
  if (99 == payload) {
    ResponseCmndChar_P(PSTR("Unlimited time not supported")); return;
  }

  SBuffer buf(3);
  buf.add16(EZSP_permitJoining);
  buf.add8(duration);
  ZigbeeEZSPSendCmd(buf.getBuffer(), buf.len());

  // send ZDO_Mgmt_Permit_Joining_req to all routers
  buf.setLen(0);
  buf.add8(duration);
  buf.add8(0x01);       // TC_Significance - This field shall always have a value of 1, indicating a request to change the Trust Center policy. If a frame is received with a value of 0, it shall be treated as having a value of 1.
  EZ_SendZDO(0xFFFC, ZDO_Mgmt_Permit_Joining_req, buf.buf(), buf.len());

  // Set Timer after the end of the period, and reset a non-expired previous timer
  if (duration > 0) {
    // Log pairing mode enabled
    Response_P(PSTR("{\"" D_JSON_ZIGBEE_STATE "\":{\"Status\":21,\"Message\":\"Pairing mode enabled\"}}"));
    MqttPublishPrefixTopicRulesProcess_P(RESULT_OR_TELE, PSTR(D_JSON_ZIGBEE_STATE));
    zigbee.permit_end_time = millis() + duration * 1000;
    AddLog_P2(LOG_LEVEL_INFO, "zigbee.permit_end_time = %d", zigbee.permit_end_time);
  } else {
    zigbee.permit_end_time = millis();
  }
#endif // USE_ZIGBEE_EZSP

  ResponseCmndDone();
}

#ifdef USE_ZIGBEE_EZSP
//
// `ZbListen`: add a multicast group to listen to
// Overcomes a current limitation that EZSP only shows messages from multicast groups it listens too
//
// Ex: `ZbListen 99`, `ZbListen2 100`
void CmndZbEZSPListen(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }

  int32_t  index = XdrvMailbox.index;   // 0 is reserved for group 0 (auto-config)
  int32_t  group = XdrvMailbox.payload;

  if (group <= 0) {
    group = 0;
  } else if (group > 0xFFFF) {
    group = 0xFFFF;
  }

  SBuffer buf(8);
  buf.add16(EZSP_setMulticastTableEntry);
  buf.add8(index);
  buf.add16(group);   // group
  buf.add8(0x01);       // endpoint
  buf.add8(0x00);       // network index
  ZigbeeEZSPSendCmd(buf.getBuffer(), buf.len());

  ResponseCmndDone();
}

void ZigbeeGlowPermitJoinLight(void) {
  static const uint16_t cycle_time = 1000;    // cycle up and down in 1000 ms
  static const uint16_t half_cycle_time = cycle_time / 2;    // cycle up and down in 1000 ms
  if (zigbee.permit_end_time) {
    uint16_t led_power = 0;         // turn led off
    // permit join is ongoing
    if (TimeReached(zigbee.permit_end_time)) {
      zigbee.permit_end_time = 0;   // disable timer
      Z_PermitJoinDisable();
    } else {
      uint32_t millis_to_go = millis() - zigbee.permit_end_time;
      uint32_t sub_second = millis_to_go % cycle_time;
      if (sub_second <= half_cycle_time) {
        led_power = changeUIntScale(sub_second, 0, half_cycle_time, 0, 1023);
      } else {
        led_power = changeUIntScale(sub_second, half_cycle_time, cycle_time, 1023, 0);
      }
      led_power = ledGamma10_10(led_power);
    }

    // change the led state
    uint32_t led_pin = Pin(GPIO_LEDLNK);
    if (led_pin < 99) {
      analogWrite(led_pin, TasmotaGlobal.ledlnk_inverted ? 1023 - led_power : led_power);
    }
  }
}
#endif // USE_ZIGBEE_EZSP

//
// Command `ZbStatus`
//
void CmndZbStatus(void) {
  if (ZigbeeSerial) {
    if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
    String dump;

    Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);
    if (XdrvMailbox.data_len > 0) {
      if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }
      dump = zigbee_devices.dumpDevice(XdrvMailbox.index, device);
    } else {
      if (XdrvMailbox.index >= 2) { ResponseCmndChar_P(PSTR("Unknown device")); return; }
      dump = zigbee_devices.dumpDevice(XdrvMailbox.index, *(Z_Device*)nullptr);
    }

    Response_P(PSTR("{\"%s%d\":%s}"), XdrvMailbox.command, XdrvMailbox.index, dump.c_str());
  }
}

//
// Innder part of ZbData parsing
//
// {"L02":{"Dimmer":10,"Sat":254}}
bool parseDeviceInnerData(class Z_Device & device, JsonParserObject root) {
  for (auto data_elt : root) {
    // Parse key in format "L02":....
    const char * data_type_str = data_elt.getStr();
    Z_Data_Type data_type;
    uint8_t endpoint;
    uint8_t config = 0xFF;    // unspecified

    // parse key in the form "L01.5"
    if (!Z_Data::ConfigToZData(data_type_str, &data_type, &endpoint, &config)) { data_type = Z_Data_Type::Z_Unknown; }

    if (data_type == Z_Data_Type::Z_Unknown) {
      Response_P(PSTR("{\"%s\":\"%s \"%s\"\"}"), XdrvMailbox.command, PSTR("Invalid Parameters"), data_type_str);
      return false;
    }

    JsonParserObject data_values = data_elt.getValue().getObject();
    if (!data_values) { return false; }

    JsonParserToken val;
    if (data_type == Z_Data_Type::Z_Device) {
      if (val = data_values[PSTR(D_CMND_ZIGBEE_LINKQUALITY)]) { device.lqi = val.getUInt(); }
      if (val = data_values[PSTR("BatteryPercentage")])       { device.batterypercent = val.getUInt(); }
      if (val = data_values[PSTR("LastSeen")])                { device.last_seen = val.getUInt(); }
    } else {
      // Import generic attributes first
      device.addEndpoint(endpoint);
      Z_Data & data = device.data.getByType(data_type, endpoint);

      // scan through attributes
      if (&data != nullptr) {
        if (config != 0xFF) {
          data.setConfig(config);
        }

        for (auto attr : data_values) {
          JsonParserToken attr_value = attr.getValue();
          uint8_t     conv_zigbee_type;
          Z_Data_Type conv_data_type;
          uint8_t     conv_map_offset;
          if (zigbeeFindAttributeByName(attr.getStr(), nullptr, nullptr, nullptr, &conv_zigbee_type, &conv_data_type, &conv_map_offset) != nullptr) {
            // found an attribute matching the name, does is fit the type?
            if (conv_data_type == data_type) {
              // we got a match. Bear in mind that a zero value is not a valid 'data_type'

              uint8_t *attr_address = ((uint8_t*)&data) + sizeof(Z_Data) + conv_map_offset;
              uint32_t uval32 = attr_value.getUInt();     // call converter to uint only once
              int32_t  ival32 = attr_value.getInt();     // call converter to int only once
              switch (conv_zigbee_type) {
                case Zenum8:
                case Zuint8:  *(uint8_t*)attr_address  = uval32;          break;
                case Zenum16:
                case Zuint16: *(uint16_t*)attr_address = uval32;          break;
                case Zuint32: *(uint32_t*)attr_address = uval32;          break;
                case Zint8:   *(int8_t*)attr_address   = ival32;          break;
                case Zint16:  *(int16_t*)attr_address  = ival32;          break;
                case Zint32:  *(int32_t*)attr_address  = ival32;          break;
              }
            } else if (conv_data_type != Z_Data_Type::Z_Unknown) {
              AddLog_P2(LOG_LEVEL_DEBUG, PSTR(D_LOG_ZIGBEE "attribute %s is wrong type %d (expected %d)"), attr.getStr(), (uint8_t)data_type, (uint8_t)conv_data_type);
            }
          }
        }
      }

      // Import specific attributes that are not handled with the generic method
      switch (data_type) {
      // case Z_Data_Type::Z_Plug:
      //   {
      //     Z_Data_Plug & plug = (Z_Data_Plug&) data;
      //   }
      //   break;
      // case Z_Data_Type::Z_Light:
      //   {
      //     Z_Data_Light & light = (Z_Data_Light&) data;
      //   }
      //   break;
      case Z_Data_Type::Z_OnOff:
        {
          Z_Data_OnOff & onoff = (Z_Data_OnOff&) data;

          if (val = data_values[PSTR("Power")])      { onoff.setPower(val.getUInt() ? true : false); }
        }
        break;
      // case Z_Data_Type::Z_Thermo:
      //   {
      //     Z_Data_Thermo & thermo = (Z_Data_Thermo&) data;
      //   }
      //   break;
      // case Z_Data_Type::Z_Alarm:
      //   {
      //     Z_Data_Alarm & alarm = (Z_Data_Alarm&) data;
      //   }
      //   break;
      }
    }
  }
  return true;
}

//
// Command `ZbData`
//
void CmndZbData(void) {
  if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  RemoveSpace(XdrvMailbox.data);
  if (XdrvMailbox.data[0] == '{') {
    // JSON input, enter saved data into memory -- essentially for debugging
    JsonParser parser(XdrvMailbox.data);
    JsonParserObject root = parser.getRootObject();
    if (!root) { ResponseCmndChar_P(PSTR(D_JSON_INVALID_JSON)); return; }

    // Skip `ZbData` if present
    JsonParserToken zbdata = root.getObject().findStartsWith(PSTR("ZbData"));
    if (zbdata) {
      root = zbdata;
    }

    for (auto device_name : root) {
      Z_Device & device = zigbee_devices.parseDeviceFromName(device_name.getStr(), true);
      if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }
      JsonParserObject inner_data = device_name.getValue().getObject();
      if (inner_data) {
        if (!parseDeviceInnerData(device, inner_data)) {
          return;
        }
      }
    }
    zigbee_devices.dirty();     // save to flash
    ResponseCmndDone();
  } else {
    // non-JSON, export current data
    // ZbData 0x1234
    // ZbData Device_Name
    Z_Device & device = zigbee_devices.parseDeviceFromName(XdrvMailbox.data, true);
    if (!device.valid()) { ResponseCmndChar_P(PSTR("Unknown device")); return; }

    Z_attribute_list attr_data;

    {   // scope to force object deallocation
      Z_attribute_list device_attr;
      device.toAttributes(device_attr);
      attr_data.addAttribute(F("_")).setStrRaw(device_attr.toString(true).c_str());
    }

    // Iterate on data objects
    for (auto & data_elt : device.data) {
      Z_attribute_list inner_attr;
      char key[8];
      if (data_elt.validConfig()) {
        snprintf_P(key, sizeof(key), "?%02X.%1X", data_elt.getEndpoint(), data_elt.getConfig());
      } else {
        snprintf_P(key, sizeof(key), "?%02X", data_elt.getEndpoint());
      }

      Z_Data_Type data_type = data_elt.getType();
      key[0] = Z_Data::DataTypeToChar(data_type);
      switch (data_type) {
        case Z_Data_Type::Z_Plug:
          ((Z_Data_Plug&)data_elt).toAttributes(inner_attr, data_type);
          break;
        case Z_Data_Type::Z_Light:
          ((Z_Data_Light&)data_elt).toAttributes(inner_attr, data_type);
          break;
        case Z_Data_Type::Z_OnOff:
          ((Z_Data_OnOff&)data_elt).toAttributes(inner_attr, data_type);
          break;
        case Z_Data_Type::Z_Thermo:
          ((Z_Data_Thermo&)data_elt).toAttributes(inner_attr, data_type);
          break;
        case Z_Data_Type::Z_Alarm:
          ((Z_Data_Alarm&)data_elt).toAttributes(inner_attr, data_type);
          break;
        case Z_Data_Type::Z_PIR:
          ((Z_Data_PIR&)data_elt).toAttributes(inner_attr, data_type);
          break;
      }
      if ((key[0] != '\0') && (key[0] != '?')) {
        attr_data.addAttribute(key).setStrRaw(inner_attr.toString(true).c_str());
      }
    }

    char hex[8];
    snprintf_P(hex, sizeof(hex), PSTR("0x%04X"), device.shortaddr);
    Response_P(PSTR("{\"%s\":{\"%s\":%s}}"), XdrvMailbox.command, hex, attr_data.toString(true).c_str());
  }
}

//
// Command `ZbConfig`
//
void CmndZbConfig(void) {
  // ZbConfig
  // ZbConfig {"Channel":11,"PanID":"0x1A63","ExtPanID":"0xCCCCCCCCCCCCCCCC","KeyL":"0x0F0D0B0907050301L","KeyH":"0x0D0C0A0806040200L"}
  uint8_t     zb_channel     = Settings.zb_channel;
  uint16_t    zb_pan_id      = Settings.zb_pan_id;
  uint64_t    zb_ext_panid   = Settings.zb_ext_panid;
  uint64_t    zb_precfgkey_l = Settings.zb_precfgkey_l;
  uint64_t    zb_precfgkey_h = Settings.zb_precfgkey_h;
  int8_t      zb_txradio_dbm = Settings.zb_txradio_dbm;

  // if (zigbee.init_phase) { ResponseCmndChar_P(PSTR(D_ZIGBEE_NOT_STARTED)); return; }
  RemoveSpace(XdrvMailbox.data);
  if (strlen(XdrvMailbox.data) > 0) {
    JsonParser parser(XdrvMailbox.data);
    JsonParserObject root = parser.getRootObject();
    if (!root) { ResponseCmndChar_P(PSTR(D_JSON_INVALID_JSON)); return; }
    // Channel

    zb_channel      = root.getUInt(PSTR("Channel"), zb_channel);
    zb_pan_id       = root.getUInt(PSTR("PanID"), zb_pan_id);
    zb_ext_panid    = root.getULong(PSTR("ExtPanID"), zb_ext_panid);
    zb_precfgkey_l  = root.getULong(PSTR("KeyL"), zb_precfgkey_l);
    zb_precfgkey_h  = root.getULong(PSTR("KeyH"), zb_precfgkey_h);
    zb_txradio_dbm  = root.getInt(PSTR("TxRadio"), zb_txradio_dbm);

    if (zb_channel < 11) { zb_channel = 11; }
    if (zb_channel > 26) { zb_channel = 26; }
    // if network key is zero, we generate a truly random key with a hardware generator from ESP
    if ((0 == zb_precfgkey_l) && (0 == zb_precfgkey_h)) {
      AddLog_P2(LOG_LEVEL_INFO, PSTR(D_LOG_ZIGBEE "generating random Zigbee network key"));
      zb_precfgkey_l = (uint64_t)HwRandom() << 32 | HwRandom();
      zb_precfgkey_h = (uint64_t)HwRandom() << 32 | HwRandom();
    }

    // Check if a parameter was changed after all
    if ( (zb_channel      != Settings.zb_channel) ||
         (zb_pan_id       != Settings.zb_pan_id) ||
         (zb_ext_panid    != Settings.zb_ext_panid) ||
         (zb_precfgkey_l  != Settings.zb_precfgkey_l) ||
         (zb_precfgkey_h  != Settings.zb_precfgkey_h) ||
         (zb_txradio_dbm  != Settings.zb_txradio_dbm) ) {
      Settings.zb_channel      = zb_channel;
      Settings.zb_pan_id       = zb_pan_id;
      Settings.zb_ext_panid    = zb_ext_panid;
      Settings.zb_precfgkey_l  = zb_precfgkey_l;
      Settings.zb_precfgkey_h  = zb_precfgkey_h;
      Settings.zb_txradio_dbm  = zb_txradio_dbm;
      TasmotaGlobal.restart_flag = 2;    // save and reboot
    }
  }

  // display the current or new configuration
  char hex_ext_panid[20] = "0x";
  Uint64toHex(zb_ext_panid, &hex_ext_panid[2], 64);
  char hex_precfgkey_l[20] = "0x";
  Uint64toHex(zb_precfgkey_l, &hex_precfgkey_l[2], 64);
  char hex_precfgkey_h[20] = "0x";
  Uint64toHex(zb_precfgkey_h, &hex_precfgkey_h[2], 64);

  // {"ZbConfig":{"Channel":11,"PanID":"0x1A63","ExtPanID":"0xCCCCCCCCCCCCCCCC","KeyL":"0x0F0D0B0907050301L","KeyH":"0x0D0C0A0806040200L"}}
  Response_P(PSTR("{\"" D_PRFX_ZB D_JSON_ZIGBEE_CONFIG "\":{"
                  "\"Channel\":%d"
                  ",\"PanID\":\"0x%04X\""
                  ",\"ExtPanID\":\"%s\""
                  ",\"KeyL\":\"%s\""
                  ",\"KeyH\":\"%s\""
                  ",\"TxRadio\":%d"
                  "}}"),
                  zb_channel, zb_pan_id,
                  hex_ext_panid,
                  hex_precfgkey_l, hex_precfgkey_h,
                  zb_txradio_dbm);
}

/*********************************************************************************************\
 * Presentation
\*********************************************************************************************/

extern "C" {
  int device_cmp(const void * a, const void * b) {
    const Z_Device &dev_a = zigbee_devices.devicesAt(*(uint8_t*)a);
    const Z_Device &dev_b = zigbee_devices.devicesAt(*(uint8_t*)b);
    const char * fn_a = dev_a.friendlyName;
    const char * fn_b = dev_b.friendlyName;

    if (fn_a && fn_b) {
      return strcasecmp(fn_a, fn_b);
    } else if (!fn_a && !fn_b) {
      return (int32_t)dev_a.shortaddr - (int32_t)dev_b.shortaddr;
    } else {
      if (fn_a) return -1;
      return 1;
    }
  }


// Convert seconds to a string representing days, hours or minutes present in the n-value.
// The string will contain the most coarse time only, rounded down (61m == 01h, 01h37m == 01h).
// Inputs:
// - n: uint32_t representing some number of seconds
// - result: a buffer of suitable size (7 bytes would represent the entire solution space
//           for UINT32_MAX including the trailing null-byte, or "49710d")
// - result_len: A numeric value representing the total length of the result buffer
// Returns:
// - The number of characters that would have been written were result sufficiently large
// - negatve number on encoding error from snprintf
//
  int convert_seconds_to_dhm(uint32_t n,  char *result, size_t result_len){
    char fmtstr[] = "%02dmhd"; // Don't want this in progmem, because we mutate it.
    uint32_t conversions[3] = {24 * 3600, 3600, 60};
    uint32_t value;
    for(int i = 0; i < 3; ++i) {
      value = n / conversions[i];
      if(value > 0) {
        fmtstr[4] = fmtstr[6-i];
        break;
      }
      n = n % conversions[i];
    }

    // Null-terminate the string at the last "valid" index, removing any excess zero values.
    fmtstr[5] = '\0';
    return snprintf(result, result_len, fmtstr, value);
  }
}
void ZigbeeShow(bool json)
{
  if (json) {
    return;
#ifdef USE_WEBSERVER
  } else {
    uint32_t zigbee_num = zigbee_devices.devicesSize();
    if (!zigbee_num) { return; }
    if (zigbee_num > 255) { zigbee_num = 255; }

    WSContentSend_P(PSTR("</table>{t}"));  // Terminate current two column table and open new table
    WSContentSend_P(PSTR(
      "<style>"
      // Table CSS
      ".ztd td:not(:first-child){width:20px;font-size:70%%}"
      ".ztd td:last-child{width:45px}"
      ".ztd .bt{margin-right:10px;}" // Margin right should be half of the not-first width
      ".htr{line-height:20px}"
      // Lighting
      ".bx{height:14px;width:14px;display:inline-block;border:1px solid currentColor;background-color:var(--cl,#fff)}"
      // Signal Strength Indicator
      ".ssi{display:inline-flex;align-items:flex-end;height:15px;padding:0}"
      ".ssi i{width:3px;margin-right:1px;border-radius:3px;background-color:#eee}"
      ".ssi .b0{height:25%%}.ssi .b1{height:50%%}.ssi .b2{height:75%%}.ssi .b3{height:100%%}.o30{opacity:.3}"
      "</style>"
    ));

    // sort elements by name, then by id
    uint8_t sorted_idx[zigbee_num];
    for (uint32_t i = 0; i < zigbee_num; i++) {
      sorted_idx[i] = i;
    }
    qsort(sorted_idx, zigbee_num, sizeof(sorted_idx[0]), device_cmp);

    uint32_t now = Rtc.utc_time;

    for (uint32_t i = 0; i < zigbee_num; i++) {
      const Z_Device &device = zigbee_devices.devicesAt(sorted_idx[i]);
      uint16_t shortaddr = device.shortaddr;
      char *name = (char*) device.friendlyName;

      char sdevice[33];
      if (nullptr == name) {
        snprintf_P(sdevice, sizeof(sdevice), PSTR(D_DEVICE " 0x%04X"), shortaddr);
        name = sdevice;
      }

      char sbatt[64];
      snprintf_P(sbatt, sizeof(sbatt), PSTR("&nbsp;"));
      if (device.validBatteryPercent()) {
        snprintf_P(sbatt, sizeof(sbatt),
          PSTR("<i class=\"bt\" title=\"%d%%\" style=\"--bl:%dpx\"></i>"),
          device.batterypercent, changeUIntScale(device.batterypercent, 0, 100, 0, 14)
        );
      }

      uint32_t num_bars = 0;

      char slqi[4];
      slqi[0] = '-';
      slqi[1] = '\0';
      if (device.validLqi()){
        num_bars = changeUIntScale(device.lqi, 0, 254, 0, 4);
        snprintf_P(slqi, sizeof(slqi), PSTR("%d"), device.lqi);
      }

      WSContentSend_PD(PSTR(
        "<tr class='ztd htr'>"
          "<td><b title='0x%04X %s - %s'>%s</b></td>" // name
          "<td>%s</td>" // sbatt (Battery Indicator)
          "<td><div title='" D_LQI " %s' class='ssi'>" // slqi
      ), shortaddr, 
      device.modelId ? device.modelId : "", 
      device.manufacturerId ? device.manufacturerId : "", 
      name, sbatt, slqi);

      if(device.validLqi()) {
          for(uint32_t j = 0; j < 4; ++j) {
            WSContentSend_PD(PSTR("<i class='b%d%s'></i>"), j, (num_bars < j) ? PSTR(" o30") : PSTR(""));
          }
      }
      char dhm[16]; // len("&#x1F557;" + "49710d" + '\0') == 16
      snprintf_P(dhm, sizeof(dhm), PSTR("&nbsp;"));
      if(device.validLastSeen()){
        snprintf_P(dhm, sizeof(dhm), PSTR("&#x1F557;"));
        convert_seconds_to_dhm(now - device.last_seen, &dhm[9], 7);
      }

      WSContentSend_PD(PSTR(
        "</div></td>" // Close LQI
        "<td>%s{e}" // dhm (Last Seen)
      ), dhm );

      // Sensors
      const Z_Data_Thermo & thermo = device.data.find<Z_Data_Thermo>();

      if (&thermo != nullptr) {
        bool validTemp = thermo.validTemperature();
        bool validTempTarget = thermo.validTempTarget();
        bool validThSetpoint = thermo.validThSetpoint();
        bool validHumidity = thermo.validHumidity();
        bool validPressure = thermo.validPressure();

        if (validTemp || validTempTarget || validThSetpoint || validHumidity || validPressure) {
          WSContentSend_P(PSTR("<tr class='htr'><td colspan=\"4\">&#9478;"));
          if (validTemp) {
            char buf[12];
            dtostrf(thermo.getTemperature() / 100.0f, 3, 1, buf);
            WSContentSend_PD(PSTR(" &#x2600;&#xFE0F; %s??C"), buf);
          }
          if (validTempTarget) {
            char buf[12];
            dtostrf(thermo.getTempTarget() / 100.0f, 3, 1, buf);
            WSContentSend_PD(PSTR(" &#127919; %s??C"), buf);
          }
          if (validThSetpoint) {
            WSContentSend_PD(PSTR(" &#9881;&#65039; %d%%"), thermo.getThSetpoint());
          }
          if (validHumidity) {
            WSContentSend_P(PSTR(" &#x1F4A7; %d%%"), (uint16_t)(thermo.getHumidity() / 100.0f + 0.5f));
          }
          if (validPressure) {
            WSContentSend_P(PSTR(" &#x26C5; %d hPa"), thermo.getPressure());
          }

          WSContentSend_P(PSTR("{e}"));
        }
      }

      // Light, switches and plugs
      const Z_Data_OnOff & onoff = device.data.find<Z_Data_OnOff>();
      const Z_Data_Light & light = device.data.find<Z_Data_Light>();
      bool light_display = (&light != nullptr) ? light.validDimmer() : false;
      const Z_Data_Plug & plug = device.data.find<Z_Data_Plug>();
      if ((&onoff != nullptr) || light_display || (&plug != nullptr)) {
        int8_t channels = device.getLightChannels();
        if (channels < 0) { channels = 5; }     // if number of channel is unknown, display all known attributes
        WSContentSend_P(PSTR("<tr class='htr'><td colspan=\"4\">&#9478;"));
        if (&onoff != nullptr) {
          WSContentSend_P(PSTR(" %s"), device.getPower() ? PSTR(D_ON) : PSTR(D_OFF));
        }
        if (&light != nullptr) {
          if (light.validDimmer() && (channels >= 1)) {
            WSContentSend_P(PSTR(" &#128261; %d%%"), changeUIntScale(light.getDimmer(),0,254,0,100));
          }
          if (light.validCT() && ((channels == 2) || (channels == 5))) {
            uint32_t ct_k = (((1000000 / light.getCT()) + 25) / 50) * 50;
            WSContentSend_P(PSTR(" <span title=\"CT %d\"><small>&#9898; </small>%dK</span>"), light.getCT(), ct_k);
          }
          if (light.validHue() && light.validSat() && (channels >= 3)) {
            uint8_t r,g,b;
            uint8_t sat = changeUIntScale(light.getSat(), 0, 254, 0, 255);    // scale to 0..255
            LightStateClass::HsToRgb(light.getHue(), sat, &r, &g, &b);
            WSContentSend_P(PSTR(" <i class=\"bx\" style=\"--cl:#%02X%02X%02X\"></i>#%02X%02X%02X"), r,g,b,r,g,b);
          } else if (light.validX() && light.validY() && (channels >= 3)) {
            uint8_t r,g,b;
            LightStateClass::XyToRgb(light.getX() / 65535.0f, light.getY() / 65535.0f, &r, &g, &b);
            WSContentSend_P(PSTR(" <i class=\"bx\" style=\"--cl:#%02X%02X%02X\"></i> #%02X%02X%02X"), r,g,b,r,g,b);
          }
        }
        if (&plug != nullptr) {
          bool validMainsVoltage = plug.validMainsVoltage();
          bool validMainsPower = plug.validMainsPower();
          if (validMainsVoltage || validMainsPower) {
            WSContentSend_P(PSTR(" &#9889; "));
            if (validMainsVoltage) {
              WSContentSend_P(PSTR(" %dV"), plug.getMainsVoltage());
            }
            if (validMainsPower) {
              WSContentSend_P(PSTR(" %dW"), plug.getMainsPower());
            }
          }
        }
        WSContentSend_P(PSTR("{e}"));
      }
    }

    WSContentSend_P(PSTR("</table>{t}"));  // Terminate current multi column table and open new table
#endif
  }
}

/*********************************************************************************************\
 * Interface
\*********************************************************************************************/

bool Xdrv23(uint8_t function)
{
  bool result = false;

  if (zigbee.active) {
    switch (function) {
      case FUNC_EVERY_50_MSECOND:
        if (!zigbee.init_phase) {
          zigbee_devices.runTimer();
        }
        break;
      case FUNC_LOOP:
#ifdef USE_ZIGBEE_EZSP
        if (ZigbeeUploadXmodem()) {
          return false;
        }
#endif
        if (ZigbeeSerial) {
          ZigbeeInputLoop();
          ZigbeeOutputLoop();   // send any outstanding data
#ifdef USE_ZIGBEE_EZSP
          ZigbeeGlowPermitJoinLight();
#endif // USE_ZIGBEE_EZSP
        }
        if (zigbee.state_machine) {
          ZigbeeStateMachine_Run();
        }
        break;
#ifdef USE_WEBSERVER
      case FUNC_WEB_SENSOR:
        ZigbeeShow(false);
        break;
#ifdef USE_ZIGBEE_EZSP
      // GUI xmodem
      case FUNC_WEB_ADD_HANDLER:
        WebServer_on(PSTR("/" WEB_HANDLE_ZIGBEE_XFER), HandleZigbeeXfer);
        break;
#endif  // USE_ZIGBEE_EZSP
#endif  // USE_WEBSERVER
      case FUNC_PRE_INIT:
        ZigbeeInit();
        break;
      case FUNC_COMMAND:
        result = DecodeCommand(kZbCommands, ZigbeeCommand);
        break;
    }
  }
  return result;
}

#endif // USE_ZIGBEE
