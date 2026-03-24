#include <stdarg.h>
#include <stdio.h>

#include "iot/logger.h"

#include "edgex-logging.h"
#include "errorlist.h"
#include "config.h"
#include "secrets.h"
//#include "secrets-impl.h"
#include "command.h"

void useRndMem(iot_logger_t * lc)
{
  // random stack/heap user to call and make sure i'm not depending on results in abandoned memory 
  if (lc==NULL) return;
  iot_log_debug(lc,"useRndMem() RAND_MAX: %d",RAND_MAX);

  double ratio=0.0;//rand()/RAND_MAX;
  const int memmax=512;
  int memused=0;
  char *tgtmem=NULL;
  int intarr[memmax];
  
  for (int i=0;i<6;++i){
    int newrand=rand();
    ratio=(double)newrand/(double)RAND_MAX;
    memused=ratio*memmax;
    tgtmem=malloc(sizeof(char)*memused);
    iot_log_debug(lc,"useRndMem() pass[%d] CrWrDel chunk size: %d/%d",i,memused,memmax);
    for (int j=0;j<memused;++j){
      int data=rand();
      intarr[j]=data;
      *(tgtmem+j)=(char)((data/RAND_MAX)*255);
    }
    free(tgtmem);
  }
  return;
}

void cmdresult_parse(iot_logger_t * lc,iot_data_t *cmdresult)
{
  const char *responseApiVer=NULL;
  const char *responseStatusCode=NULL;
  iot_data_t *responseEventMap=NULL;
  devsdk_commandresult responseResult;
  if (cmdresult)
  {
    responseApiVer=iot_data_string_map_get_string(cmdresult,"apiVersion");
    iot_log_debug(lc,"response ApiVersion: %s",responseApiVer);
    responseStatusCode=iot_data_string_map_get_string(cmdresult,"statusCode");
    // check the cmd result
    // populate a devsdk_commandresult
    responseEventMap=iot_data_string_map_get_map(cmdresult,"event");

    responseResult.origin=iot_data_string_map_get_ui64(responseEventMap,"origin",0u);
    responseResult.value=iot_data_string_map_get_vector(responseEventMap,"readings");
    int nreadings=0;
    if (responseResult.value!=NULL) 
    {
      nreadings=iot_data_vector_size(responseResult.value);
      iot_log_debug(lc,"readings length: %d",nreadings);
      iot_data_t *readingMap=NULL;
      const char *dName=NULL;
      const char *pName=NULL;
      const char *rName=NULL;
      const char *valtype=NULL;
      const char *val=NULL;
      iot_log_debug(lc,"readings data type: %s",iot_data_type_string(iot_data_type(responseResult.value)));
      iot_log_debug(lc,"readings vector data type: %s",iot_data_type_string(iot_data_vector_type(responseResult.value)));
      for (int i=0;i<nreadings;++i){
        readingMap=iot_data_vector_get(responseResult.value,i);
        if ((readingMap==NULL)||(iot_data_type(readingMap)!=IOT_DATA_MAP)) {
          iot_log_error(lc,"reading[%d] parse failed",i);
          continue;
        }
        dName=iot_data_string_map_get_string(readingMap,"deviceName");
        pName=iot_data_string_map_get_string(readingMap,"profileName");
        rName=iot_data_string_map_get_string(readingMap,"resourceName");
        valtype=iot_data_string_map_get_string(readingMap,"valueType");
        val=iot_data_string_map_get_string(readingMap,"value");
        iot_log_debug(lc,"reading[%d] %s.%s.%s ( type='%s' value='%s')",i,dName,pName,rName,valtype,val);
      }
    }
  }
}

/* data struct examples:

"deviceCoreCommands": [
  deviceCoreCommand01,
  deviceCoreCommand02,
  .
  .,
]

"deviceCoreCommand":{
      "deviceName": "Cmdclient1",
      "profileName": "Example-CmdClient",
      "coreCommands": [
      ]
},

"coreCommands": [
  coreCommand01,
  coreCommand02,
  .
  .,
  {
    "name": "AllCounters",
    "get": true,
    "set": true,
    "path": "/api/v3/device/name/Counter2/AllCounters",
    "url": "http://edgex-core-command:59882",
    "parameters": [
      {
        "resourceName": "Counter01",
        "valueType": "Uint32"
      },
      {
        "resourceName": "Counter02",
        "valueType": "Uint32"
      }
    ]
  }
]

"coreCommand": {
  "name": "Counter02",
  "get": true,
  "set": true,
  "path": "/api/v3/device/name/Counter2/Counter02",
  "url": "http://edgex-core-command:59882",
  "parameters": [
    {
      "resourceName": "Counter02",
      "valueType": "Uint32"
    }
  ]
}
*/

void devcorecmd_parse(iot_logger_t * lc,iot_data_t *cmdresult)
{
    if (cmdresult==NULL)
    {
      iot_log_error(lc,"failed to parse deviceCoreCommand"); 
      return;
    }
    iot_data_t *coreCmdsVec=NULL;
    iot_data_t *coreCmd=NULL;
  
    iot_log_debug(lc,"deviceName: '%s', profileName: '%s'",iot_data_string_map_get_string(cmdresult,"deviceName"),
                     iot_data_string_map_get_string(cmdresult,"profileName"));

    coreCmdsVec=iot_data_string_map_get_vector(cmdresult,"coreCommands");

    int coreCmdTotal=0;
    if (coreCmdsVec!=NULL) 
    {
      iot_data_t *coreCommandMap=NULL;
      const char *cmdname=NULL;
      bool cmdget=false;
      bool cmdset=false;
      const char *cmdpath=NULL;
      const char *cmdurl=NULL;
      iot_data_t *cmdparamvec=NULL;
      iot_data_t *cmdparammap=NULL;
      const char *cmdparamname=NULL;
      const char *cmdparamtype=NULL;

      iot_log_debug(lc,"commands:");
      coreCmdTotal=iot_data_vector_size(coreCmdsVec);
      for (int i=0;i<coreCmdTotal;++i){
        coreCommandMap=iot_data_vector_get(coreCmdsVec,i);
        if ((coreCommandMap==NULL)||(iot_data_type(coreCommandMap)!=IOT_DATA_MAP)) {
          iot_log_error(lc,"coreCommand[%d] parse failed",i);
          continue;
        }
        cmdname=iot_data_string_map_get_string(coreCommandMap,"name");
        cmdget=iot_data_string_map_get_bool(coreCommandMap,"get",false);
        cmdset=iot_data_string_map_get_bool(coreCommandMap,"set",false);
        cmdpath=iot_data_string_map_get_string(coreCommandMap,"path");
        cmdurl=iot_data_string_map_get_string(coreCommandMap,"url");
        iot_log_debug(lc,"  name: %s",cmdname);
        iot_log_debug(lc,"  get: %s, set: %s",(cmdget ? "true" : "false"),(cmdset ? "true" : "false"));
        iot_log_debug(lc,"  path: %s",cmdpath);
        iot_log_debug(lc,"  url: %s",cmdurl);
        iot_log_debug(lc,"  params:");

        
        cmdparamvec=iot_data_string_map_get_vector(coreCommandMap,"parameters");
        int paramcount=iot_data_vector_size(cmdparamvec);
        for (int j=0;j<paramcount;++j){
          cmdparammap=iot_data_vector_get(cmdparamvec,j);
          cmdparamname=iot_data_string_map_get_string(cmdparammap,"resourceName");
          cmdparamtype=iot_data_string_map_get_string(cmdparammap,"valueType");
          iot_log_debug(lc,"    { resourceName: %s, valueType: %s }",cmdparamname,cmdparamtype);
        }
      }
    }
}

void devcorecmdresp_parse(iot_logger_t * lc,iot_data_t *cmdresult)
{
  const char *responseApiVer=NULL;
  const char *responseStatusCode=NULL;
  int responseTotalCount=0;
  iot_data_t *devCoreCmd=NULL;
  iot_data_t *coreCmdsVec=NULL;
  iot_data_t *coreCmd=NULL;
  if (cmdresult)
  {
    responseApiVer=iot_data_string_map_get_string(cmdresult,"apiVersion");
    responseStatusCode=iot_data_string_map_get_string(cmdresult,"statusCode");
    iot_log_debug(lc,"response: %d, ApiVersion: %s ",responseStatusCode,responseApiVer);

    // parse a deviceCoreCommand
    devCoreCmd=iot_data_string_map_get_map(cmdresult,"deviceCoreCommand");


    devcorecmd_parse(lc,devCoreCmd);

  }
}

void devcorecmds_parse(iot_logger_t * lc,iot_data_t *cmdresult)
{
  const char *responseApiVer=NULL;
  const char *responseStatusCode=NULL;
  const char *responseTotalCount=NULL;
  //int responseTotalCount=0;

  iot_data_t *corecmdvec=NULL;
  int corecmdtotal=0;//iot_data_vector_size(coreCmdsVec);
  
  if (cmdresult)
  {
    responseApiVer=iot_data_string_map_get_string(cmdresult,"apiVersion");
    responseStatusCode=iot_data_string_map_get_string(cmdresult,"statusCode");
    responseTotalCount=iot_data_string_map_get_string(cmdresult,"totalCount");
    iot_log_debug(lc,"response: %d, ApiVersion: %s, totalCount: %s ",responseStatusCode,responseApiVer,responseTotalCount);
    
    corecmdvec=iot_data_string_map_get_vector(cmdresult,"deviceCoreCommands");
    if (corecmdvec) {
      iot_data_t *devcorecmd=NULL;
      corecmdtotal=iot_data_vector_size(corecmdvec);
      iot_log_debug(lc,"deviceCoreCommands total: %d ",corecmdtotal);
      for (int i=0;i<corecmdtotal;++i){
        devcorecmd=iot_data_vector_get(corecmdvec,i);
        devcorecmd_parse(lc,devcorecmd);
      }
    }
    //devcorecmd_parse(lc,cmdresult,true);
  }
  
}

int main(int argc, char** argv)
{
  iot_loglevel_t ll = IOT_LOG_TRACE;
  const char loggername[]="sdkc_cmdclient_test\0";
  iot_logger_t * lc = iot_logger_alloc_custom (loggername, ll, true, NULL, edgex_log_tostdout, (void *)loggername, NULL);


  iot_log_debug(lc,"*** manual command client read test start ***");
  const char cmdhost[] = "localhost\0";
  const char devicestr[] ="Counter2\0";  
  const char resourcestr[] ="Counter02\0";
  const char cmdstr[] ="AllCounters\0";

  edgex_service_endpoints * endpoints= malloc(sizeof(edgex_service_endpoints));
  endpoints->command.host=cmdhost;
  endpoints->command.port=59882;
  endpoints->metadata.host=cmdhost;
  endpoints->metadata.port=59881;

  edgex_secret_provider_t * secretprovider=edgex_secrets_get_insecure();

  const char * devicename = devicestr;
  const char * resourcename = cmdstr;
  const iot_data_t * cmd = NULL;
  bool pusheventflag=false;
  bool returneventflag=true;
  iot_data_t* cmdresult=NULL;
  devsdk_error err;

  cmdresult = edgex_command_read_command( lc, endpoints, secretprovider, devicename, resourcename,
                                          pusheventflag, returneventflag, &err );

  if (err.code == 0) {
    // allocate, fill, deallocate, etc, make/some variables to make sure i'm not reading from abandoned mem
    useRndMem(lc);
    cmdresult_parse(lc,cmdresult);
    iot_log_debug(lc,"manual edgex_command_read_command() ok");
  }
  else
  {
    iot_log_error(lc,"manual edgex_command_read_command() failed");
    return 1;
  }

  iot_data_free(cmdresult);
  cmdresult=NULL;

  iot_log_debug(lc,"*** manual command client get devices commands test start ***");

  cmdresult = edgex_command_get_device_commands( lc, endpoints, secretprovider, devicename, &err );

  if (err.code == 0) {
    
    useRndMem(lc);
    devcorecmdresp_parse(lc,cmdresult);
    iot_log_debug(lc,"manual edgex_command_get_device_commands() ok");
  }
  else
  {
    iot_log_error(lc,"manual edgex_command_get_device_commands() failed");
    return 2;
  }

  iot_data_free(cmdresult);
  cmdresult=NULL;

  iot_log_debug(lc,"*** manual command client all commands test start ***");

  cmdresult = edgex_command_get_all_commands( lc, endpoints, secretprovider, 1, 20, &err );

  if (err.code == 0) {
    
    //useRndMem(lc);
    devcorecmds_parse(lc,cmdresult);
    iot_log_debug(lc,"manual edgex_command_get_all_commands() ok");
  }
  else
  {
    iot_log_error(lc,"manual edgex_command_get_all_commands() failed");
    return 2;
  }

  iot_data_free(cmdresult);
  cmdresult=NULL;

  iot_log_debug(lc,"*** manual command client tests complete ***");

  if (endpoints) free(endpoints);

  edgex_secrets_fini (secretprovider);
  iot_logger_free (lc);
  return 0;
}