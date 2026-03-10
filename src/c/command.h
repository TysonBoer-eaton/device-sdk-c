/*
 * Copyright (c) 2024
 * Eaton Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#ifndef _EDGEX_COMMAND_H_
#define _EDGEX_COMMAND_H_ 1

#include "device.h"
#include "devsdk/devsdk.h"
#include "devsdk/devsdk-base.h"
#include "iot/logger.h"
#include "devmap.h"
#include "parson.h"
#include "secrets.h"

typedef struct edgex_service_endpoints edgex_service_endpoints;

void devsdk_devicecorecommand_populate(const iot_data_t *map,uint32_t *nc,devsdk_devicecorecommand **dcc);

void devsdk_corecommand_populate(const iot_data_t *map,devsdk_corecommand **cc);

void edgex_command_write_command
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  const char * resourcename,
  const iot_data_t * cmd,
  devsdk_error * err
);

iot_data_t *edgex_command_read_command
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  const char * commandname,
  bool pushevent,
  bool returnevent,
  devsdk_error * err
);

iot_data_t *edgex_command_get_device_commands
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  devsdk_error * err
);

iot_data_t *edgex_command_get_all_commands
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  int offset,
  int limit,
  devsdk_error * err
);

#endif