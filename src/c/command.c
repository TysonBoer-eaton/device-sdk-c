/*
 * Copyright (c) 2024
 * Eaton Corp.
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <curl/curl.h>
#include <errno.h>

#include "api.h"
#include "command.h"
#include "config.h"
#include "edgex-rest.h"
#include "errorlist.h"
#include "rest.h"
#include "service.h"

#include "edgex/devices.h"

/** devsdk api functions, defined in devsdk.h **/

void devsdk_get_commands(devsdk_service_t *svc, int offset, int limit, uint32_t *ncommands, const devsdk_devicecorecommand **commands, devsdk_error *err)
{
  *err = EDGEX_OK;
  devsdk_devicecorecommand *cmds=*commands;
  iot_data_t *device_commands=NULL;
  iot_data_t *response_map=edgex_command_get_all_commands
  (
    svc->logger,
    &svc->config.endpoints,
    svc->secretstore,
    offset,
    limit,
    err
  );
  if (err->code){
    iot_log_error (svc->logger, "Failed to get commands. Reason: %s",
      err->reason);
      *ncommands=0;
      memset(cmds,0,sizeof(devsdk_devicecorecommand));
      return;
  }

  device_commands=iot_data_string_map_get_vector(response_map,"deviceCoreCommands");
  if (!device_commands)
  {
    iot_log_error (svc->logger, "Failed to get commands. Can not read 'deviceCoreCommands'");
    *ncommands=0;
    memset(cmds,0,sizeof(devsdk_devicecorecommand));
    return;
  }

  iot_data_vector_iter_t viter;
  iot_data_vector_iter(device_commands,&viter);
  devsdk_devicecorecommand *newcc=cmds;
  devsdk_devicecorecommand **ccend=&cmds->next;
  while (iot_data_vector_iter_next(&viter))
  {
    if (!newcc)
    {
      newcc = malloc(sizeof(devsdk_corecommand));
      memset(cmds,0,sizeof(devsdk_devicecorecommand));
      *ccend=newcc;
    }
    devsdk_devicecorecommand_populate(iot_data_vector_iter_value(&viter),ncommands,&newcc);
    ccend=&newcc->next;
    newcc=NULL;
  }
}

void devsdk_get_commands_by_name(devsdk_service_t *svc, const char *devname, uint32_t *ncommands, const devsdk_devicecorecommand **commands, devsdk_error *err)
{
  *err = EDGEX_OK;
  devsdk_devicecorecommand *cmdparse=*commands;
  iot_data_t *device_commands=NULL;
  iot_data_t *map=edgex_command_get_device_commands
  (
    svc->logger,
    &svc->config.endpoints,
    svc->secretstore,
    devname,
    err
  );
  if (err->code){
    iot_log_error (svc->logger, "Failed to get commands for device %s. Reason: %s",
      devname,
      err->reason);
      *ncommands=0;
      cmdparse=NULL;
      return;
  }

  device_commands=iot_data_string_map_get_map(map,"deviceCoreCommand");
  if (!device_commands)
  {
    iot_log_error (svc->logger, "Failed to get commands for device %s. Reason: core-command invalid map",devname);
    *ncommands=0;
    memset(cmdparse,0,sizeof(devsdk_devicecorecommand));
    return;
  }
  devsdk_devicecorecommand_populate(device_commands,ncommands,&cmdparse);
}

void devsdk_send_command (devsdk_service_t *svc, const char *devname, const char *resname, const iot_data_t *cmd, devsdk_error *err)
{
  *err = EDGEX_OK;
  edgex_device *device;
  device = edgex_devmap_device_byname (svc->devices, devname);
  if (device)
  {
    iot_data_t* reply;
    iot_data_t* request;
    iot_data_t* value;

    request = iot_data_alloc_map (IOT_DATA_STRING);
    char* json = iot_data_to_json(cmd);
    value = (iot_data_type(cmd) == IOT_DATA_STRING) ? iot_data_copy(cmd) : iot_data_alloc_string(json, IOT_DATA_REF);

    iot_data_string_map_add(request, resname, value);

    int32_t result = edgex_device_v3impl (svc, device, resname, false, request, NULL, &reply, false);
    if (result != 0) *err = EDGEX_HTTP_ERROR;

    iot_data_free(request);
    free(json);
  }
  else
  {
    edgex_command_write_command
    (
      svc->logger,
      &svc->config.endpoints,
      svc->secretstore,
      devname,
      resname,
      cmd,
      err
    );
  }
  if (err->code)
  {
    iot_log_error (svc->logger, "Unable to send command to %s device %s resource %s. Reason: %s",
      device ? "internal" : "external",
      devname,
      resname,
      err->reason);
  }
}

void devsdk_read_command (
  devsdk_service_t *svc,
  const char *devname,
  const char *cmdname,
  bool pushevent,
  bool returnevent,
  uint32_t **nreadings,
  const devsdk_commandresult **readings,
  devsdk_error *err
)
{
  *err = EDGEX_OK;
  edgex_device *device;
  device = edgex_devmap_device_byname (svc->devices, devname);

  uint32_t *resulttotal = *nreadings;
  devsdk_commandresult *cmdreadings=*readings;

  iot_data_t *readcommanddata = edgex_command_read_command(
      svc->logger,
      &svc->config.endpoints,
      svc->secretstore,
      devname,
      cmdname,
      false,
      true,
      err
  );
  if (err->code)
  {
    iot_log_error (svc->logger, "Failed read command for %s device %s resource %s. Reason: %s",
      device ? "internal" : "external",
      devname,
      cmdname,
      err->reason);
      *nreadings=0;
      cmdreadings=NULL;
      return;
  }

  iot_data_t *responseEventMap=iot_data_string_map_get_map(readcommanddata,"event");
  cmdreadings->origin=iot_data_string_map_get_ui64(responseEventMap,"origin",0u);
  cmdreadings->value=iot_data_string_map_get_vector(responseEventMap,"readings");
  *resulttotal=0;
  if (cmdreadings->value!=NULL)
  {
      *resulttotal=iot_data_vector_size(cmdreadings->value);
  }
}

void devsdk_devicecorecommand_populate(const iot_data_t *map,uint32_t *nc,devsdk_devicecorecommand **dcc)
{
  if (map==NULL) return;

  devsdk_devicecorecommand *d=*dcc;

  d->deviceName=iot_data_string_map_get_string(map,"deviceName");
  d->profileName=iot_data_string_map_get_string(map,"profileName");
  d->corecommands=NULL;
  d->next=NULL;

  iot_data_vector_iter_t viter;
  iot_data_vector_iter(iot_data_string_map_get_vector(map,"coreCommands"),&viter);
  devsdk_corecommand *newcc=NULL;
  devsdk_corecommand **ccend=&d->corecommands;
  while (iot_data_vector_iter_next(&viter))
  {
    newcc = malloc(sizeof(devsdk_corecommand));
    memset(newcc,0,sizeof(devsdk_corecommand));
    devsdk_corecommand_populate(iot_data_vector_iter_value(&viter),&newcc);
    *ccend=newcc;
    ccend=&newcc->next;
    ++(*nc);
  }
}

void devsdk_corecommand_populate(const iot_data_t *map,devsdk_corecommand **cc)
{
  if (map==NULL) return;

  devsdk_corecommand *c=*cc;

  c->name=iot_data_string_map_get_string(map,"name");
  c->get=iot_data_string_map_get_bool(map,"get",false);
  c->set=iot_data_string_map_get_bool(map,"set",false);
  c->path=iot_data_string_map_get_string(map,"path");
  c->url=iot_data_string_map_get_string(map,"url");
  c->parameters=iot_data_string_map_get_pointer(map,"parameters");
  c->next=NULL;
}

void devsdk_corecommand_free(devsdk_corecommand *c)
{
  free(c->name);
  free(c->path);
  free(c->url);
  iot_data_free(c->parameters);
  c->next=NULL;
}

void devsdk_devicecorecommand_free(devsdk_devicecorecommand *dcc)
{
  if (dcc->next) devsdk_devicecorecommand_free(dcc->next);
  free(dcc->deviceName);
  free(dcc->profileName);
  devsdk_corecommand *ccs=dcc->corecommands;
  devsdk_corecommand *nextcc=NULL;
  if (ccs)
  {
    nextcc=ccs->next;
    devsdk_corecommand_free(ccs);
    ccs=nextcc;
  }
}

/** internal edgex access functions  **/

void edgex_command_write_command
(
  iot_logger_t *lc,
  edgex_service_endpoints *endpoints,
  edgex_secret_provider_t *secretprovider,
  const char *devicename,
  const char *resourcename,
  const iot_data_t *cmd,
  devsdk_error *err
)
{
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));
  char *json = edgex_command_write (resourcename, cmd);

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/name/%s/%s", endpoints->command.host, endpoints->command.port, devicename, resourcename);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_put (lc, &ctx, url, json, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  json_free_serialized_string (json);
  free (ctx.buff);
}

iot_data_t *edgex_command_read_command
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  const char * commandname,
  bool pusheventflag,
  bool returneventflag,
  devsdk_error * err
)
{
  iot_data_t *result=NULL;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  // this is ugly and needs to be refactored, or added as a function to edgex-rest.h/c
  const char * querytrue="true\0";
  const char * queryfalse="false\0";
  const char * pushevent=queryfalse;
  const char * returnevent=queryfalse;
  if (pusheventflag) {
    pushevent=querytrue;
  }
  if (returneventflag){
    returnevent=querytrue;
  }

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/name/%s/%s?ds-pushevent=%s&ds-returnevent=%s",
            endpoints->command.host, endpoints->command.port, devicename, commandname,pushevent,returnevent);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    result = iot_data_from_json (ctx.buff);
  }
  free (ctx.buff);

  return result;
}

iot_data_t *edgex_command_get_device_commands
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  const char * devicename,
  devsdk_error * err
)
{
  iot_data_t *result=NULL;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/name/%s",
            endpoints->command.host, endpoints->command.port, devicename);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    result = iot_data_from_json (ctx.buff);
  }
  free (ctx.buff);

  return result;
}

iot_data_t *edgex_command_get_all_commands
(
  iot_logger_t * lc,
  edgex_service_endpoints * endpoints,
  edgex_secret_provider_t * secretprovider,
  int offset,
  int limit,
  devsdk_error * err
)
{
  iot_data_t *result=NULL;
  edgex_ctx ctx;
  char url[URL_BUF_SIZE];

  memset (&ctx, 0, sizeof (edgex_ctx));

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/all?limit=%d&offset=%d",
            endpoints->command.host, endpoints->command.port, limit,offset);

  iot_data_t *jwt_data = edgex_secrets_request_jwt (secretprovider);
  ctx.jwt_token = iot_data_string(jwt_data);

  edgex_http_get (lc, &ctx, url, edgex_http_write_cb, err);

  iot_data_free(jwt_data);
  ctx.jwt_token = NULL;

  if (err->code == 0)
  {
    result = iot_data_from_json (ctx.buff);
  }
  free (ctx.buff);

  return result;
}