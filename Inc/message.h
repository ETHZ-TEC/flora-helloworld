/*
 * Copyright (c) 2018 - 2021, ETH Zurich, Computer Engineering Group (TEC)
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __MESSAGE_H
#define __MESSAGE_H


/* --- definitions --- */


/* --- typedefs --- */

typedef struct {
  dpp_command_type_t  type;
  uint16_t            arg;
  uint32_t            scheduled_time;
} scheduled_cmd_t;

typedef struct {
  uint32_t starttime;
  uint32_t period;
} periodic_t;


/* --- function prototypes --- */

bool      process_command(const dpp_command_t* cmd, const dpp_header_t* hdr);
bool      process_message(dpp_message_t* msg, bool rcvd_from_bolt);
void      process_scheduled_commands(void);
bool      send_message(dpp_message_type_t type);
void      generate_command(dpp_command_type_t cmd, uint16_t arg);
bool      schedule_command(uint32_t sched_time, dpp_command_type_t cmd_type, uint16_t arg);
uint32_t  get_next_timestamp_at_daytime(time_t curr_time, uint32_t hour, uint32_t minute, uint32_t second);


#endif /* __MESSAGE_H */
