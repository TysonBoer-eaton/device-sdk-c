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

#define BOOL_FMT(x) ((x) ? "true" : "false")

/** devsdk api functions, defined in devsdk.h **/
void devsdk_get_commands(devsdk_service_t *svc, int offset, int limit, uint32_t *ncommands, iot_data_t **commands, devsdk_error *err)
{
  *err = EDGEX_OK;

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
      *commands=NULL;
      return;
  }

  const iot_data_t *src_dev_commands=iot_data_string_map_get_vector(response_map,"deviceCoreCommands");
  if (!src_dev_commands)
  {
    iot_log_error (svc->logger, "Failed to get commands. Can not read 'deviceCoreCommands'");
    *commands=NULL;
    return;
  }

  uint32_t ncc=0,i=0;
  *commands=iot_data_alloc_vector(iot_data_vector_size(src_dev_commands));
  iot_data_t *devcommand_vector=*commands;
  devsdk_devicecorecommand *newdcc=NULL;
  iot_data_vector_iter_t viter;
  iot_data_vector_iter(src_dev_commands,&viter);
  while (iot_data_vector_iter_next(&viter))
  {
    ncc=0;
    devsdk_devicecorecommand_populate(iot_data_vector_iter_value(&viter),&ncc,&newdcc);
    iot_data_t *dccptr=iot_data_alloc_pointer(newdcc,devsdk_devicecorecommand_free);
    iot_data_vector_add(devcommand_vector,i,dccptr);
    (*ncommands)+=ncc;
    ++i;
  }
  iot_data_free(response_map);
}

void devsdk_get_commands_by_name(devsdk_service_t *svc, const char *devname, uint32_t *ncommands, devsdk_devicecorecommand **commands, devsdk_error *err)
{
  *err = EDGEX_OK;
  *ncommands=0;

  const iot_data_t *device_commands=NULL;
  iot_data_t *response_map=edgex_command_get_device_commands
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
      *commands=NULL;
      return;
  }

  device_commands=iot_data_string_map_get_map(response_map,"deviceCoreCommand");
  if (!device_commands)
  {
    iot_log_error (svc->logger, "Failed to get commands for device %s. Reason: core-command invalid map",devname);
    return;
  }
  devsdk_devicecorecommand_populate(device_commands,ncommands,commands);
  iot_data_free(response_map);
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
    value = (iot_data_type(cmd) == IOT_DATA_STRING) ? iot_data_add_ref (cmd) : iot_data_alloc_string (iot_data_to_json (cmd), IOT_DATA_TAKE);

    iot_data_string_map_add(request, resname, value);

    int32_t result = edgex_device_v3impl (svc, device, resname, false, request, NULL, &reply, false);
    if (result != 0) *err = EDGEX_HTTP_ERROR;

    iot_data_free(request);
    //free(json);
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
  devsdk_commandresult **readings,
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

    if (readcommanddata) {
      free(readcommanddata);
    }
    *nreadings=0;
    cmdreadings=NULL;
    return;
  }

  const iot_data_t *responseEventMap=iot_data_string_map_get_map(readcommanddata,"event");
  cmdreadings->origin=iot_data_string_map_get_ui64(responseEventMap,"origin",0u);
  cmdreadings->value=iot_data_copy(iot_data_string_map_get_vector(responseEventMap,"readings"));
  *resulttotal=0;
  if (cmdreadings->value!=NULL)
  {
      *resulttotal=iot_data_vector_size(cmdreadings->value);
  }
  if (readcommanddata) {
    iot_data_free(readcommanddata);
  }
}

void devsdk_devicecorecommand_populate(const iot_data_t *map,uint32_t *nc, devsdk_devicecorecommand **dcc)
{
  if (map==NULL) return;

  *dcc=calloc(1,sizeof(devsdk_devicecorecommand));

  devsdk_devicecorecommand *d=*dcc;

  d->deviceName=strdup(iot_data_string_map_get_string(map,"deviceName"));
  d->profileName=strdup(iot_data_string_map_get_string(map,"profileName"));
  d->corecommands=NULL;
  d->next=NULL;

  *nc=0;
  iot_data_vector_iter_t viter;
  iot_data_t* cc_src_vector=iot_data_string_map_get_vector(map,"coreCommands");
  iot_data_vector_iter(cc_src_vector,&viter);
  // alloc an iot_data_vector at vector or list
  d->corecommands=iot_data_alloc_vector(iot_data_vector_size(cc_src_vector));
  devsdk_corecommand *newcc=NULL;
  while (iot_data_vector_iter_next(&viter))
  {
    devsdk_corecommand_populate(iot_data_vector_iter_value(&viter),&newcc);
    // turn into an iot_data_pointer
    iot_data_t *newcc_ptr=iot_data_alloc_pointer (newcc, devsdk_corecommand_free);
    // add to the vector
    iot_data_vector_add(d->corecommands,*nc,newcc_ptr);
    ++(*nc);
  }
}

void devsdk_corecommand_populate(const iot_data_t *map,devsdk_corecommand **cc)
{
  if (map==NULL) return;

  *cc=calloc(1,sizeof(devsdk_corecommand));

  devsdk_corecommand *c=*cc;

  c->name=strdup(iot_data_string_map_get_string(map,"name"));
  c->get=iot_data_string_map_get_bool(map,"get",false);
  c->set=iot_data_string_map_get_bool(map,"set",false);
  c->path=strdup(iot_data_string_map_get_string(map,"path"));
  c->url=strdup(iot_data_string_map_get_string(map,"url"));
  c->parameters=iot_data_copy(iot_data_string_map_get_pointer(map,"parameters"));
}

void devsdk_corecommand_free(void *v)
{
  devsdk_corecommand *c=(devsdk_corecommand *)v;
  free(c->name);
  free(c->path);
  free(c->url);
  iot_data_free(c->parameters);
  free(c);
}

void devsdk_devicecorecommand_free(void *v)
{
  devsdk_devicecorecommand *dcc=(devsdk_devicecorecommand *)v;
  free(dcc->deviceName);
  free(dcc->profileName);
  iot_data_free(dcc->corecommands);
  free(dcc);
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

  snprintf (url, URL_BUF_SIZE - 1, "http://%s:%u/api/" EDGEX_API_VERSION "/device/name/%s/%s?ds-pushevent=%s&ds-returnevent=%s",
            endpoints->command.host, endpoints->command.port, devicename, commandname,BOOL_FMT(pusheventflag),BOOL_FMT(returneventflag));

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