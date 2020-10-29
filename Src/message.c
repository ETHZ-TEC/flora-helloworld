/*
 * message.c
 *
 * functions related to DPP message handling / processing
 *
 *  Created on: Apr 7, 2020
 *      Author: rdaforno
 */

#include "main.h"


/* Private variables ---------------------------------------------------------*/

static dpp_message_t msg_buffer;

#if BASEBOARD
LIST_CREATE(pending_commands, sizeof(scheduled_cmd_t), COMMAND_QUEUE_SIZE);
#endif /* BASEBOARD */


/* Functions -----------------------------------------------------------------*/

bool process_command(const dpp_command_t* cmd, const dpp_header_t* hdr)
{
  bool cfg_changed = false;
#if BASEBOARD
  scheduled_cmd_t sched_cmd;
#endif /* BASEBOARD */

  if (!cmd) {
    return false;
  }

  switch (cmd->type) {

  case DPP_COMMAND_RESET:
  case CMD_SX1262_RESET:
    LOG_WARNING("resetting...");
    NVIC_SystemReset();
    break;

#if BASEBOARD
  case CMD_SX1262_BASEBOARD_ENABLE:
  case CMD_SX1262_BASEBOARD_DISABLE:
    sched_cmd.type           = cmd->type;
    sched_cmd.scheduled_time = cmd->arg32[0];
    sched_cmd.arg            = 0;
    if (cmd->arg[4] & 1) {
      /* relative time -> append generation time */
      if (hdr) {
        sched_cmd.scheduled_time += hdr->generation_time / 1000000;
      }
    }
    if (cmd->type == CMD_SX1262_BASEBOARD_ENABLE) {
      sched_cmd.arg = (uint16_t)cmd->arg[6] << 8 | cmd->arg[5];
    }
    if (!list_insert(pending_commands, sched_cmd.scheduled_time, &sched_cmd)) {
      LOG_WARNING("failed to add command to queue");
    } else {
      LOG_VERBOSE("baseboard command %u scheduled (time: %lu)", cmd->type & 0xff, sched_cmd.scheduled_time);
    }
    break;

  case CMD_SX1262_BASEBOARD_ENABLE_PERIODIC:
    config.bb_en.period = (uint32_t)cmd->arg16[1] * 60;  /* convert to seconds */
    if (config.bb_en.period > 0) {
      config.bb_en.starttime = get_next_timestamp_at_daytime(0, cmd->arg[0], cmd->arg[1], 0);
      if (config.bb_en.starttime > 0) {
        LOG_INFO("periodic baseboard enable scheduled (next: %u  period: %us)", config.bb_en.starttime, config.bb_en.period);
      } else {
        LOG_WARNING("invalid parameters for periodic enable cmd");
      }
    } else {
      config.bb_en.starttime = 0;
      LOG_INFO("periodic baseboard enable cleared");
    }
    cfg_changed = true;
    break;

  case CMD_SX1262_BASEBOARD_POWER_EXT3:
    if (cmd->arg[0]) {
      PIN_SET(BASEBOARD_EXT3_SWITCH);
      LOG_INFO("EXT3 power enabled");
    } else {
      PIN_CLR(BASEBOARD_EXT3_SWITCH);
      LOG_INFO("EXT3 power disabled");
    }
    break;
#endif /* BASEBOARD */

  default:
    /* unknown command -> command processing failed */
    return false;
    break;
  }

  if (cfg_changed) {
    if (!nvcfg_save(&config)) {
      LOG_ERROR("failed to save config");
    }
  }

  return true;
}


/* returns true if message is valid */
bool process_message(dpp_message_t* msg, bool rcvd_from_bolt)
{
  uint16_t msg_len = DPP_MSG_LEN(msg);

  /* check message type, length and CRC */
  if (!msg ||
      msg->header.type & DPP_MSG_TYPE_MIN ||
      msg_len > DPP_MSG_PKT_LEN           ||
      msg->header.payload_len == 0        ||
      DPP_MSG_GET_CRC16(msg) != crc16((uint8_t*)msg, msg_len - 2, 0)) {
    LOG_ERROR("msg with invalid length or CRC (src: %u  len: %ub  type: %u)", msg->header.device_id, msg_len, msg->header.type);
    return false;
  }
  LOG_VERBOSE("msg type: %u  src: %u  len: %uB   target: %u", msg->header.type, msg->header.device_id, msg_len, msg->header.target_id);

  /* only process the message if target ID matched the node ID */
  if ((msg->header.target_id == NODE_ID) || (msg->header.target_id == DPP_DEVICE_ID_BROADCAST)) {
    if (msg->header.type == DPP_MSG_TYPE_CMD) {
      LOG_VERBOSE("command received");
      /* command successfully executed? */
      if (process_command(&msg->cmd, &msg->header)) {
        LOG_INFO("cmd %u processed", msg->cmd.type & 0xff);
      } else {
        LOG_WARNING("failed to process cmd %u", msg->cmd.type & 0xff);
      }
      return true;

    /* message types only processed by the host */
    } else if (msg->header.type == DPP_MSG_TYPE_TIMESYNC) {
      LOG_VERBOSE("timestamp %llu received", msg->timestamp);
      set_time(msg->timestamp);
      return true;

    /* unknown message type */
    } else {
      LOG_WARNING("message of unknown type %u received", msg->header.type);
    }
  } else {
    LOG_WARNING("message of type %u ignored", msg->header.type);
  }

  return false;
}


#if BASEBOARD

void process_scheduled_commands(void)
{
  uint32_t curr_time = rtc_get_unix_timestamp();
  const scheduled_cmd_t* next_cmd = list_get_head(pending_commands);
  if (next_cmd) {
    /* there are pending commands */
    /* anything that needs to be executed now? */
    while (next_cmd && next_cmd->scheduled_time <= curr_time) {
      switch (next_cmd->type) {
      case CMD_SX1262_BASEBOARD_ENABLE:
        PIN_SET(BASEBOARD_ENABLE);
        LOG_INFO("baseboard enabled");
        generate_command(CMD_BASEBOARD_WAKEUP_MODE, next_cmd->arg);
        break;
      case CMD_SX1262_BASEBOARD_DISABLE:
        PIN_CLR(BASEBOARD_ENABLE);
        LOG_INFO("baseboard disabled");
        break;
      default:
        break;
      }
      list_remove_head(pending_commands, 0);
      next_cmd = list_get_head(pending_commands);
    }
  }
  /* check the periodic baseboard enable */
  if (config.bb_en.starttime > 0 && config.bb_en.starttime <= curr_time) {
    PIN_SET(BASEBOARD_ENABLE);
    while (config.bb_en.period > 0 && config.bb_en.starttime < curr_time) {
      config.bb_en.starttime += config.bb_en.period;
    }
    LOG_INFO("baseboard enabled (next wakeup in %us)", config.bb_en.period);
  }
}

#endif /* BASEBOARD */


/*
 * send message to BOLT
 */
bool send_message(dpp_message_type_t type)
{
  /* separate sequence number for each interface */
  static uint16_t seq_no = 0;
  uint32_t        len    = 0;

  /* compose the message header */
  msg_buffer.header.device_id   = NODE_ID;
  msg_buffer.header.type        = type;
  msg_buffer.header.payload_len = len;
  switch(type) {
  case DPP_MSG_TYPE_COM_HEALTH:
    msg_buffer.header.payload_len = sizeof(dpp_com_health_t); break;
  case DPP_MSG_TYPE_CMD:
    msg_buffer.header.payload_len = 6; break;  /* default is 6 bytes */
  case DPP_MSG_TYPE_EVENT:
    msg_buffer.header.payload_len = sizeof(dpp_event_t); break;
  case DPP_MSG_TYPE_NODE_INFO:
    msg_buffer.header.payload_len = sizeof(dpp_node_info_t); break;
  case DPP_MSG_TYPE_TIMESYNC:
    msg_buffer.header.payload_len = sizeof(dpp_timestamp_t); break;
  default:
    LOG_WARNING("unknown message type");
    return false;
    break;
  }
  msg_buffer.header.target_id = NODE_ID;
  msg_buffer.header.seqnr = seq_no++;
  msg_buffer.header.generation_time = get_time(0);

  /* calculate and append the CRC */
  uint32_t msg_buffer_len = DPP_MSG_LEN(&msg_buffer);
  uint16_t crc = crc16((uint8_t*)&msg_buffer, msg_buffer_len - 2, 0);
  DPP_MSG_SET_CRC16(&msg_buffer, crc);

  if (!bolt_write((uint8_t*)&msg_buffer, msg_buffer_len)) {
    LOG_WARNING("msg dropped (BOLT queue full)");
    return false;
  }
  LOG_VERBOSE("msg of type %u and length %u bytes written to BOLT", type, msg_buffer_len);
  return true;
}


/* send a command with a 2-byte argument to the app processor */
void generate_command(dpp_command_type_t cmd, uint16_t arg)
{
  msg_buffer.cmd.type     = cmd;
  msg_buffer.cmd.arg16[0] = arg;
  send_message(DPP_MSG_TYPE_CMD);
}


#if BASEBOARD

bool schedule_command(uint32_t sched_time, dpp_command_type_t cmd_type, uint16_t arg)
{
  scheduled_cmd_t cmd;

  cmd.scheduled_time = sched_time;
  cmd.type           = cmd_type;
  cmd.arg            = arg;

  return list_insert(pending_commands, sched_time, &cmd);
}

#endif /* BASEBOARD */


uint32_t get_next_timestamp_at_daytime(time_t curr_time, uint32_t hour, uint32_t minute, uint32_t second)
{
  struct tm ts;

  /* make sure the values are within the valid range */
  if (hour > 23 || minute > 59 || second > 59) {
    return 0;
  }
  /* if no time specified, fetch current time */
  if (curr_time == 0) {
    curr_time = (time_t)(get_time(0) / 1000000);
  }
  /* split up the UNIX timestamp into components */
  gmtime_r(&curr_time, &ts);
  ts.tm_hour = hour;
  ts.tm_min  = minute;
  ts.tm_sec  = second;
  /* convert back to UNIX timestamp */
  uint32_t timestamp = mktime(&ts);
  if (timestamp <= curr_time) {
    timestamp += 86400;           /* add one day */
  }
  return timestamp;
}

