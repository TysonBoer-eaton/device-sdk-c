/* Pseudo-device service emulating counters using C SDK */

/*
 * Copyright (c) 2018-2020
 * IoTech Ltd
 *
 * SPDX-License-Identifier: Apache-2.0
 *
 */

#include <unistd.h>
#include <signal.h>
#include <stdarg.h>

#include "devsdk/devsdk.h"
#include "iot/logger.h"

#define ERR_CHECK(x) if (x.code) { fprintf (stderr, "Error: %d: %s\n", x.code, x.reason); devsdk_service_free (service); free (impl); return x.code; }

typedef enum { COUNTER_R0 } counter_register;

devsdk_service_t *service;

typedef struct cmdclient_driver
{
  iot_logger_t * lc;
  devsdk_service_t *svc;
} cmdclient_driver;

static bool cmdclient_init
  (void *impl, struct iot_logger_t *lc, const iot_data_t *config)
{
  cmdclient_driver *driver = (cmdclient_driver *) impl;
  driver->lc = lc;
  return true;
}

static devsdk_address_t cmdclient_create_addr (void *impl, const devsdk_protocols *protocols, iot_data_t **exception)
{
  iot_data_t *addrstr=iot_data_alloc_string("commandclient",IOT_DATA_REF);
  return addrstr;
}

static void cmdclient_free_addr (void *impl, devsdk_address_t address)
{
  free (address);
}

static devsdk_resource_attr_t cmdclient_create_resource_attr (void *impl, const iot_data_t *attributes, iot_data_t **exception)
{
  iot_data_t * command_client_map = iot_data_alloc_map (IOT_DATA_STRING);
  const char *srcdev = iot_data_string_map_get_string (attributes, "sourceDevice");
  const char *srcres = iot_data_string_map_get_string (attributes, "sourceResource");

  if (srcdev){
    iot_data_string_map_add (command_client_map, "sourceDevice", iot_data_alloc_string (srcdev, IOT_DATA_COPY));
  }
  if (srcres){
    iot_data_string_map_add (command_client_map, "sourceResource", iot_data_alloc_string (srcres, IOT_DATA_COPY));
  }

  int offset = 0;
  int limit = 0;
  iot_data_string_map_get_int (attributes, "offset",&offset);
  iot_data_string_map_get_int (attributes, "limit",&limit);
  iot_data_string_map_add (command_client_map, "offset", iot_data_alloc_i32 (offset));
  iot_data_string_map_add (command_client_map, "limit", iot_data_alloc_i32 (limit));

  return command_client_map;
}

static void cmdclient_free_resource_attr (void *impl, devsdk_resource_attr_t resource)
{
  if (resource!=NULL) iot_data_free (resource);
}

static bool cmdclient_get_handler
(
 void *impl,
 const devsdk_device_t *device,
 uint32_t nreadings,
 const devsdk_commandrequest *requests,
 devsdk_commandresult *readings,
 iot_data_t **tags,
 const iot_data_t *options,
 iot_data_t **exception
)
{
  cmdclient_driver *driver = (cmdclient_driver *)impl;
  uint64_t index = *(uint64_t *)device->address;

  const char *srcdev = NULL;// expected iot_data_string_map_get_string (attributes, "sourceDevice");
  const char *srcres = NULL;// expected iot_data_string_map_get_string (attributes, "sourceResource");
  int offset=0;
  int limit=0;
  devsdk_error cmderror;

  devsdk_commandresult *ccreadings=readings;
  iot_data_t *dev_corecommand_list=NULL;
  devsdk_devicecorecommand *devcorecommands=NULL;

  uint32_t ccnum=0;
  uint32_t *ccnumptr=&ccnum;
  devsdk_error cc_err;

  if (driver->svc==NULL) {
    *exception = iot_data_alloc_string ("Device service not available, cannot access core-command", IOT_DATA_REF);
    return false;
  }

  for (uint32_t i = 0; i < nreadings; i++)
  {
    iot_log_debug(driver->lc,"processing request[%d] of %d",i,nreadings);
    if (requests[i].resource->attrs!=NULL && iot_data_type(requests[i].resource->attrs)==IOT_DATA_MAP)
    {
      srcdev = iot_data_string_map_get_string (requests[i].resource->attrs, "sourceDevice");
      srcres = iot_data_string_map_get_string (requests[i].resource->attrs, "sourceResource");
      iot_data_string_map_get_int (requests[i].resource->attrs, "offset",&offset);
      iot_data_string_map_get_int (requests[i].resource->attrs, "limit",&limit);

      iot_data_t *commandlist=NULL;

      if ((srcdev!=NULL)&&(srcres!=NULL)) {
        iot_log_debug(driver->lc,"  requesting from core-command device '%s': resource '%s'",srcdev,srcres);
        devsdk_read_command (driver->svc,
          srcdev,srcres,false,true,
          &ccnumptr,
          &ccreadings,
          &cc_err);
        iot_log_debug(driver->lc,"  core-command returns %d readings",ccnum);
      }
      else if ((srcdev!=NULL)&&(srcres==NULL)) {
        iot_log_debug(driver->lc,"  requesting from core-command device '%s' command list",srcdev);

        devcorecommands=NULL;

        devsdk_get_commands_by_name (driver->svc,
          srcdev,
          &ccnum,
          &devcorecommands,
          &cc_err);
        iot_log_debug(driver->lc,"  core-command returns %d commands",ccnum);

        devsdk_devicecorecommand *dcc=devcorecommands;
        if (dcc)
        {
          iot_log_debug(driver->lc,"device name: %s, device profile: %s",dcc->deviceName,dcc->profileName);
        }

        devsdk_corecommand *cc=NULL;
        iot_data_vector_iter_t viter;
        iot_data_vector_iter(devcorecommands->corecommands,&viter);
        int i=0;
        while (iot_data_vector_iter_next(&viter))
        {
          cc=(devsdk_corecommand*)iot_data_vector_get_pointer(devcorecommands->corecommands,i);
          iot_log_debug(driver->lc,"    command[%d]: '%s'",i,cc->name);
          ++i;
        }

        readings=NULL;

        devsdk_devicecorecommand_free(devcorecommands);
        return false; // until we figure out how to parse
      }
      else if ((srcdev==NULL)&&(srcres==NULL)) {
        iot_log_debug(driver->lc,"  requesting from core-command all commands offset: %d, limit: %d",offset,limit);

        iot_data_t* devcc_list=NULL;

        devsdk_get_commands (driver->svc,
          offset,limit,
          &ccnum,
          &devcc_list,
          &cc_err);
        iot_log_debug(driver->lc,"  core-command returns %d commands",ccnum);
        readings=NULL;
        devsdk_devicecorecommand *dcc=NULL;
        for (int i=0;i<iot_data_vector_size(devcc_list);++i)
        {
          dcc=(devsdk_devicecorecommand*)iot_data_vector_get_pointer(devcc_list,i);
          iot_log_debug(driver->lc,"device name: %s, device profile: %s",dcc->deviceName,dcc->profileName);
        }
        iot_data_free(devcc_list);

        return false; // until we figure out how to parse
      }
      
    }
    else 
    {
      iot_log_debug(driver->lc,"  unexpected attr found, type: %s",iot_data_type_string(iot_data_type(requests[i].resource->attrs)));
    }
  }

  if (cc_err.code!=0) {
    return false;
  }
  return true;
  
}

static bool cmdclient_put_handler
(
  void *impl,
  const devsdk_device_t *device,
  uint32_t nvalues,
  const devsdk_commandrequest *requests,
  const iot_data_t *values[],
  const iot_data_t *options,
  iot_data_t **exception
)
{
  cmdclient_driver *driver = (cmdclient_driver *)impl;

  // TODO: populate

  return false;
}

/* ---- Stop ---- */
/* Stop performs any final actions before the device service is terminated */
static void cmdclient_stop (void *impl, bool force) {}

int main (int argc, char *argv[])
{
  sigset_t set;
  int sigret;

  cmdclient_driver * impl = malloc (sizeof (cmdclient_driver));
  impl->lc = NULL;
  impl->svc=NULL;

  devsdk_error e;
  e.code = 0;

  devsdk_callbacks *counterImpls = devsdk_callbacks_init
  (
    cmdclient_init,
    cmdclient_get_handler,
    cmdclient_put_handler,
    cmdclient_stop,
    cmdclient_create_addr,
    cmdclient_free_addr,
    cmdclient_create_resource_attr,
    cmdclient_free_resource_attr
  );

  service = devsdk_service_new
    ("device-cmdclient", "1.0", impl, counterImpls, &argc, argv, &e);
  ERR_CHECK (e);
  impl->svc=service;

  int n = 1;
  while (n < argc)
  {
    if (strcmp (argv[n], "-h") == 0 || strcmp (argv[n], "--help") == 0)
    {
      printf ("Options:\n");
      printf ("  -h, --help\t\t\tShow this text\n");
      return 0;
    }
    else
    {
      printf ("%s: Unrecognized option %s\n", argv[0], argv[n]);
      return 0;
    }
  }

  devsdk_service_start (service, NULL, &e);
  ERR_CHECK (e);

  /****************TEST SECTION *******************************/

  //******* get by name
  uint32_t ccnum=0;
  uint32_t *ccnumptr=&ccnum;
  devsdk_error cc_err;

  //const devsdk_devicecorecommand *devcorecommands=malloc(sizeof(devsdk_devicecorecommand));
  iot_data_t *devcorecommand_list=NULL;
  devsdk_devicecorecommand *devcorecommands=NULL;
  iot_log_debug(impl->lc,"core-command tgt pointer: %X",&devcorecommands);
  //memset(devcorecommands,0,sizeof(devsdk_devicecorecommand));

  devcorecommands=NULL;
  devsdk_get_commands_by_name (service,
    "Counter2",
    &ccnum,
    &devcorecommands,
    &cc_err);
  iot_log_debug(impl->lc,"devsdk_get_commands_by_name(..,Counter2,..) core-command returns %d commands",ccnum);

  if (cc_err.code==0)
  {
    if (devcorecommands)
    {
      iot_log_debug(impl->lc,"device name: %s, device profile: %s",devcorecommands->deviceName,devcorecommands->profileName);

      devsdk_corecommand *cc=NULL;
      iot_data_vector_iter_t viter;
      iot_data_vector_iter(devcorecommands->corecommands,&viter);
      int i=0;
      while (iot_data_vector_iter_next(&viter))
      {
        //cc=(devsdk_corecommand*)iot_data_vector_iter_value(&viter);
        cc=(devsdk_corecommand*)iot_data_vector_get_pointer(devcorecommands->corecommands,i);
        iot_log_debug(impl->lc,"    command[%d]: '%s'",i,cc->name);
        ++i;
      }
    } else
    {
      iot_log_error(impl->lc,"devcorecommands is NULL");
    }
  }
  if (devcorecommands) devsdk_devicecorecommand_free(devcorecommands);

  devsdk_get_commands_by_name (service,
    "Counter1",
    &ccnum,
    &devcorecommands,
    &cc_err);
  iot_log_debug(impl->lc,"devsdk_get_commands_by_name(..,Counter1,..) core-command returns %d commands",ccnum);

  if (cc_err.code==0)
  {
    if (devcorecommands)
    {
      iot_log_debug(impl->lc,"device name: %s, device profile: %s",devcorecommands->deviceName,devcorecommands->profileName);

      devsdk_corecommand *cc=NULL;
      iot_data_vector_iter_t viter;
      iot_data_vector_iter(devcorecommands->corecommands,&viter);
      int i=0;
      while (iot_data_vector_iter_next(&viter))
      {
        //cc=(devsdk_corecommand*)iot_data_vector_iter_value(&viter);
        cc=(devsdk_corecommand*)iot_data_vector_get_pointer(devcorecommands->corecommands,i);
        iot_log_debug(impl->lc,"    command[%d]: '%s'",i,cc->name);
        ++i;
      }
    } else
    {
      iot_log_error(impl->lc,"devcorecommands is NULL");
    }
  }
  if (devcorecommands) devsdk_devicecorecommand_free(devcorecommands); // valgrind error?

  devsdk_get_commands_by_name (service,
    "AllCounters",
    &ccnum,
    &devcorecommands,
    &cc_err);
  iot_log_debug(impl->lc,"devsdk_get_commands_by_name(..,AllCounters,..) core-command returns %d commands",ccnum);

  if (cc_err.code==0)
  {
    if (devcorecommands)
    {
      iot_log_debug(impl->lc,"device name: %s, device profile: %s",devcorecommands->deviceName,devcorecommands->profileName);

      devsdk_corecommand *cc=NULL;
      iot_data_vector_iter_t viter;
      iot_data_vector_iter(devcorecommands->corecommands,&viter);
      int i=0;
      while (iot_data_vector_iter_next(&viter))
      {
        //cc=(devsdk_corecommand*)iot_data_vector_iter_value(&viter);
        cc=(devsdk_corecommand*)iot_data_vector_get_pointer(devcorecommands->corecommands,i);
        iot_log_debug(impl->lc,"    command[%d]: '%s'",i,cc->name);
        ++i;
      }
    } else
    {
      iot_log_error(impl->lc,"devcorecommands is NULL");
    }
  }
  if (devcorecommands) devsdk_devicecorecommand_free(devcorecommands); // valgrind error?


  //******* get all
  int offset=0;
  int limit=8;
  devsdk_get_commands (service,
    offset,limit,
    &ccnum,
    &devcorecommand_list,
    &cc_err);
  iot_log_debug(impl->lc,"devsdk_get_commands(...,%d,%d,...) core-command returns %d commands",offset,limit,ccnum);

  if (!devcorecommands) iot_log_debug(impl->lc,"devcorecommands size: %d",iot_data_vector_size(devcorecommand_list));

  devsdk_devicecorecommand *dcc=NULL;
  for (int i=0;i<iot_data_vector_size(devcorecommand_list);++i)
  {
    dcc=(devsdk_devicecorecommand*)iot_data_vector_get_pointer(devcorecommand_list,i);
    if (!dcc) {
      continue;
    }
    iot_log_debug(impl->lc,"dcc Addr %X",dcc->deviceName);
    iot_log_debug(impl->lc,"device name: %s, device profile: %s",dcc->deviceName,dcc->profileName);

      devsdk_corecommand *cc=NULL;
      iot_data_vector_iter_t viter;
      iot_data_vector_iter(dcc->corecommands,&viter);
      int i=0;
      while (iot_data_vector_iter_next(&viter))
      {
        cc=(devsdk_corecommand*)iot_data_vector_iter_pointer_value(&viter);
        //cc=(devsdk_corecommand*)iot_data_vector_get_pointer(dcc->corecommands,i);
        iot_log_debug(impl->lc,"    command[%d]: '%s'",i,cc->name);
        ++i;
      }
  }
  if (devcorecommand_list) iot_data_free(devcorecommand_list);

  /***********************************************************/

  sigemptyset (&set);
  sigaddset (&set, SIGINT);
  sigprocmask (SIG_BLOCK, &set, NULL);
  sigwait (&set, &sigret);
  sigprocmask (SIG_UNBLOCK, &set, NULL);

  devsdk_service_stop (service, true, &e);
  ERR_CHECK (e);

  devsdk_service_free (service);
  free (impl);
  free (counterImpls);
  return 0;
}
