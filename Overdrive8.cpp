#include <stdio.h>
#include <tchar.h>
#include <vector>
#include <sstream>
#include <string>
#include <fstream>

///
///  Copyright (c) 2018 - 2021 Advanced Micro Devices, Inc.
 
///  THIS CODE AND INFORMATION IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
///  EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
///  WARRANTIES OF MERCHANTABILITY AND/OR FITNESS FOR A PARTICULAR PURPOSE.



#include <windows.h>
#include "adl_sdk.h"
#include "adl_structures.h"
#include <stdio.h>

// #define PRINTF
#define PRINTF printf

struct OverdriveRangeDataStruct
{
    //Minimum value
    int Min_;
    //Maximum value
    int Max_;
    //Expected value: similar to current value
    int ExpectedValue_;
    //Default value
    int DefaultValue_;
    //Actual value
    int ActualValue_;
    // If ActualValue can be got from the driver, ActualValueAvailable_ will be true
    bool ActualValueAvailable_;
    // If the disable/enable feature is supported by the driver, it is true.
    bool EnableDisableSupport_;
    // The enabled state
    bool Visible_;
};

// Definitions of the used function pointers. Add more if you use other ADL APIs
typedef int(*ADL2_MAIN_CONTROL_CREATE)(ADL_MAIN_MALLOC_CALLBACK, int, ADL_CONTEXT_HANDLE*);
typedef int(*ADL2_MAIN_CONTROL_DESTROY)(ADL_CONTEXT_HANDLE);
typedef int(*ADL2_ADAPTER_NUMBEROFADAPTERS_GET)(ADL_CONTEXT_HANDLE, int*);
typedef int(*ADL2_ADAPTER_ADAPTERINFO_GET)(ADL_CONTEXT_HANDLE, LPAdapterInfo lpInfo, int iInputSize);
typedef int(*ADL2_ADAPTER_ACTIVE_GET) (ADL_CONTEXT_HANDLE, int, int*);
typedef int(*ADL2_OVERDRIVE_CAPS) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int * iSupported, int * iEnabled, int * iVersion);
typedef int(*ADL2_ADAPTER_REGVALUEINT_GET) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int iDriverPathOption, const char* szSubKey, const char *szKeyName, int *lpKeyValue);

typedef int(*ADL2_OVERDRIVE8_INIT_SETTING_GET) (ADL_CONTEXT_HANDLE, int, ADLOD8InitSetting*);
typedef int(*ADL2_OVERDRIVE8_CURRENT_SETTING_GET) (ADL_CONTEXT_HANDLE, int, ADLOD8CurrentSetting*);
typedef int(*ADL2_OVERDRIVE8_SETTING_SET) (ADL_CONTEXT_HANDLE, int, ADLOD8SetSetting*, ADLOD8CurrentSetting*);
typedef int(*ADL2_NEW_QUERYPMLOGDATA_GET) (ADL_CONTEXT_HANDLE, int, ADLPMLogDataOutput*);

typedef int(*ADL2_OVERDRIVE8_INIT_SETTINGX2_GET) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int* lpOverdrive8Capabilities, int *lpNumberOfFeatures, ADLOD8SingleInitSetting** lppInitSettingList);
typedef int(*ADL2_OVERDRIVE8_CURRENT_SETTINGX2_GET) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, int *lpNumberOfFeatures, int** lppCurrentSettingList);


typedef int(*ADL2_ADAPTER_PMLOG_SUPPORT_GET) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLPMLogSupportInfo* pPMLogSupportInfo);
typedef int(*ADL2_ADAPTER_PMLOG_SUPPORT_START) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADLPMLogStartInput* pPMLogStartInput, ADLPMLogStartOutput* pPMLogStartOutput, ADL_D3DKMT_HANDLE pDevice);
typedef int(*ADL2_ADAPTER_PMLOG_SUPPORT_STOP) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADL_D3DKMT_HANDLE pDevice);
typedef int(*ADL2_DESKTOP_DEVICE_CREATE) (ADL_CONTEXT_HANDLE context, int iAdapterIndex, ADL_D3DKMT_HANDLE *pDevice);
typedef int(*ADL2_DESKTOP_DEVICE_DESTROY) (ADL_CONTEXT_HANDLE context, ADL_D3DKMT_HANDLE hDevice);

HINSTANCE hDLL;

ADL2_MAIN_CONTROL_CREATE          ADL2_Main_Control_Create = NULL;
ADL2_MAIN_CONTROL_DESTROY         ADL2_Main_Control_Destroy = NULL;
ADL2_ADAPTER_NUMBEROFADAPTERS_GET ADL2_Adapter_NumberOfAdapters_Get = NULL;
ADL2_ADAPTER_ADAPTERINFO_GET      ADL2_Adapter_AdapterInfo_Get = NULL;
ADL2_ADAPTER_ACTIVE_GET          ADL2_Adapter_Active_Get = NULL;
ADL2_OVERDRIVE_CAPS              ADL2_Overdrive_Caps = NULL;
ADL2_ADAPTER_REGVALUEINT_GET     ADL2_Adapter_RegValueInt_Get = NULL;

ADL2_OVERDRIVE8_INIT_SETTING_GET ADL2_Overdrive8_Init_Setting_Get = NULL;
ADL2_OVERDRIVE8_CURRENT_SETTING_GET ADL2_Overdrive8_Current_Setting_Get = NULL;
ADL2_OVERDRIVE8_SETTING_SET ADL2_Overdrive8_Setting_Set = NULL;
ADL2_NEW_QUERYPMLOGDATA_GET ADL2_New_QueryPMLogData_Get = NULL;

ADL2_OVERDRIVE8_INIT_SETTINGX2_GET ADL2_Overdrive8_Init_SettingX2_Get = NULL;
ADL2_OVERDRIVE8_CURRENT_SETTINGX2_GET ADL2_Overdrive8_Current_SettingX2_Get = NULL;

ADL2_ADAPTER_PMLOG_SUPPORT_GET ADL2_Adapter_PMLog_Support_Get = NULL;
ADL2_ADAPTER_PMLOG_SUPPORT_START ADL2_Adapter_PMLog_Support_Start = NULL;
ADL2_ADAPTER_PMLOG_SUPPORT_STOP ADL2_Adapter_PMLog_Support_Stop = NULL;
ADL2_DESKTOP_DEVICE_CREATE ADL2_Desktop_Device_Create = NULL;
ADL2_DESKTOP_DEVICE_DESTROY ADL2_Desktop_Device_Destroy = NULL;

// Memory allocation function
void* __stdcall ADL_Main_Memory_Alloc ( int iSize )
{
    void* lpBuffer = malloc ( iSize );
    return lpBuffer;
}

// Optional Memory de-allocation function
void __stdcall ADL_Main_Memory_Free ( void** lpBuffer )
{
    if ( NULL != *lpBuffer )
    {
        free ( *lpBuffer );
        *lpBuffer = NULL;
    }
}

ADL_CONTEXT_HANDLE context = NULL;
LPAdapterInfo   lpAdapterInfo = NULL;
int  iNumberAdapters;

char * ADLOD8FeatureControlStr[]
{
	"ADL_OD8_GFXCLK_LIMITS",
	"ADL_OD8_GFXCLK_CURVE",
	"ADL_OD8_UCLK_MAX",
	"ADL_OD8_POWER_LIMIT",
	"ADL_OD8_ACOUSTIC_LIMIT_SCLK FanMaximumRpm",
	"ADL_OD8_FAN_SPEED_MIN FanMinimumPwm",
	"ADL_OD8_TEMPERATURE_FAN FanTargetTemperature",
	"ADL_OD8_TEMPERATURE_SYSTEM MaxOpTemp",
	"ADL_OD8_MEMORY_TIMING_TUNE",
	"ADL_OD8_FAN_ZERO_RPM_CONTROL",
	"ADL_OD8_AUTO_UV_ENGINE",
	"ADL_OD8_AUTO_OC_ENGINE",
	"ADL_OD8_AUTO_OC_MEMORY",
	"ADL_OD8_FAN_CURVE ",
	"ADL_OD8_WS_AUTO_FAN_ACOUSTIC_LIMIT",
    "ADL_OD8_GFXCLK_QUADRATIC_CURVE",
    "ADL_OD8_OPTIMIZED_GPU_POWER_MODE",
    "ADL_OD8_ODVOLTAGE_LIMIT",
	"ADL_OD8_POWER_GAUGE"
};

char *ADLOD8SettingIdStr[] =
{
	"OD8_GFXCLK_FMAX",
	"OD8_GFXCLK_FMIN",
	"OD8_GFXCLK_FREQ1",
	"OD8_GFXCLK_VOLTAGE1",
	"OD8_GFXCLK_FREQ2",
	"OD8_GFXCLK_VOLTAGE2",
	"OD8_GFXCLK_FREQ3",
	"OD8_GFXCLK_VOLTAGE3",
	"OD8_UCLK_FMAX",
	"OD8_POWER_PERCENTAGE",
	"OD8_FAN_MIN_SPEED",
	"OD8_FAN_ACOUSTIC_LIMIT",
	"OD8_FAN_TARGET_TEMP",
	"OD8_OPERATING_TEMP_MAX",
	"OD8_AC_TIMING",
	"OD8_FAN_ZERORPM_CONTROL",
	"OD8_AUTO_UV_ENGINE_CONTROL",
	"OD8_AUTO_OC_ENGINE_CONTROL",
	"OD8_AUTO_OC_MEMORY_CONTROL",
	"OD8_FAN_CURVE_TEMPERATURE_1",
	"OD8_FAN_CURVE_SPEED_1",
	"OD8_FAN_CURVE_TEMPERATURE_2",
	"OD8_FAN_CURVE_SPEED_2",
	"OD8_FAN_CURVE_TEMPERATURE_3",
	"OD8_FAN_CURVE_SPEED_3",
	"OD8_FAN_CURVE_TEMPERATURE_4",
	"OD8_FAN_CURVE_SPEED_4",
	"OD8_FAN_CURVE_TEMPERATURE_5",
	"OD8_FAN_CURVE_SPEED_5",
	"OD8_WS_FAN_AUTO_FAN_ACOUSTIC_LIMIT",
    "OD8_GFXCLK_CURVE_COEFFICIENT_A",
    "OD8_GFXCLK_CURVE_COEFFICIENT_B",
    "OD8_GFXCLK_CURVE_COEFFICIENT_C",
    "OD8_GFXCLK_CURVE_VFT_FMIN",
    "OD8_UCLK_FMIN",
    "OD8_FAN_ZERO_RPM_STOP_TEMPERATURE",
    "OD8_OPTIMZED_POWER_MODE",
	"OD8_POWER_GAUGE",
	"OD8_COUNT"
};


char *sensorType[] = {
"SENSOR_MAXTYPES",
"PMLOG_CLK_GFXCLK",
"PMLOG_CLK_MEMCLK",
"PMLOG_CLK_SOCCLK",
"PMLOG_CLK_UVDCLK1",
"PMLOG_CLK_UVDCLK2",
"PMLOG_CLK_VCECLK",
"PMLOG_CLK_VCNCLK",
"PMLOG_TEMPERATURE_EDGE",
"PMLOG_TEMPERATURE_MEM",
"PMLOG_TEMPERATURE_VRVDDC",
"PMLOG_TEMPERATURE_VRMVDD",
"PMLOG_TEMPERATURE_LIQUID",
"PMLOG_TEMPERATURE_PLX",
"PMLOG_FAN_RPM",
"PMLOG_FAN_PERCENTAGE",
"PMLOG_SOC_VOLTAGE",
"PMLOG_SOC_POWER",
"PMLOG_SOC_CURRENT",
"PMLOG_INFO_ACTIVITY_GFX",
"PMLOG_INFO_ACTIVITY_MEM",
"PMLOG_GFX_VOLTAGE",
"PMLOG_MEM_VOLTAGE",
"PMLOG_ASIC_POWER",
"PMLOG_TEMPERATURE_VRSOC",
"PMLOG_TEMPERATURE_VRMVDD0",
"PMLOG_TEMPERATURE_VRMVDD1",
"PMLOG_TEMPERATURE_HOTSPOT",
"PMLOG_TEMPERATURE_GFX",
"PMLOG_TEMPERATURE_SOC",
"PMLOG_GFX_POWER",
"PMLOG_GFX_CURRENT",
"PMLOG_TEMPERATURE_CPU",
"PMLOG_CPU_POWER",
"PMLOG_CLK_CPUCLK",
"PMLOG_THROTTLER_STATUS",
"PMLOG_CLK_VCN1CLK1",
"PMLOG_CLK_VCN1CLK2",
"PMLOG_SMART_POWERSHIFT_CPU",
"PMLOG_SMART_POWERSHIFT_DGPU",
"ADL_PMLOG_BUS_SPEED",
"ADL_PMLOG_BUS_LANES"
};

int initializeADL()
{
    
    // Load the ADL dll
    hDLL = LoadLibrary(TEXT("atiadlxx.dll"));
    if (hDLL == NULL)
    {
        // A 32 bit calling application on 64 bit OS will fail to LoadLibrary.
        // Try to load the 32 bit library (atiadlxy.dll) instead
        hDLL = LoadLibrary(TEXT("atiadlxy.dll"));
    }
    
    if (NULL == hDLL)
    {
        PRINTF("Failed to load ADL library\n");
        return FALSE;
    }
    ADL2_Main_Control_Create = (ADL2_MAIN_CONTROL_CREATE)GetProcAddress(hDLL, "ADL2_Main_Control_Create");
    ADL2_Main_Control_Destroy = (ADL2_MAIN_CONTROL_DESTROY)GetProcAddress(hDLL, "ADL2_Main_Control_Destroy");
    ADL2_Adapter_NumberOfAdapters_Get = (ADL2_ADAPTER_NUMBEROFADAPTERS_GET)GetProcAddress(hDLL, "ADL2_Adapter_NumberOfAdapters_Get");
    ADL2_Adapter_AdapterInfo_Get = (ADL2_ADAPTER_ADAPTERINFO_GET)GetProcAddress(hDLL, "ADL2_Adapter_AdapterInfo_Get");
    ADL2_Adapter_Active_Get = (ADL2_ADAPTER_ACTIVE_GET)GetProcAddress(hDLL, "ADL2_Adapter_Active_Get");
    ADL2_Overdrive_Caps = (ADL2_OVERDRIVE_CAPS)GetProcAddress(hDLL, "ADL2_Overdrive_Caps");
    ADL2_Adapter_RegValueInt_Get = (ADL2_ADAPTER_REGVALUEINT_GET)GetProcAddress(hDLL, "ADL2_Adapter_RegValueInt_Get");

    ADL2_Overdrive8_Init_Setting_Get = (ADL2_OVERDRIVE8_INIT_SETTING_GET)GetProcAddress(hDLL, "ADL2_Overdrive8_Init_Setting_Get");
    ADL2_Overdrive8_Current_Setting_Get = (ADL2_OVERDRIVE8_CURRENT_SETTING_GET)GetProcAddress(hDLL, "ADL2_Overdrive8_Current_Setting_Get");
    ADL2_Overdrive8_Setting_Set = (ADL2_OVERDRIVE8_SETTING_SET)GetProcAddress(hDLL, "ADL2_Overdrive8_Setting_Set");
    ADL2_New_QueryPMLogData_Get = (ADL2_NEW_QUERYPMLOGDATA_GET)GetProcAddress(hDLL, "ADL2_New_QueryPMLogData_Get");

    ADL2_Overdrive8_Init_SettingX2_Get = (ADL2_OVERDRIVE8_INIT_SETTINGX2_GET)GetProcAddress(hDLL, "ADL2_Overdrive8_Init_SettingX2_Get");
    ADL2_Overdrive8_Current_SettingX2_Get = (ADL2_OVERDRIVE8_CURRENT_SETTINGX2_GET)GetProcAddress(hDLL, "ADL2_Overdrive8_Current_SettingX2_Get");

	ADL2_Adapter_PMLog_Support_Get = (ADL2_ADAPTER_PMLOG_SUPPORT_GET)GetProcAddress(hDLL, "ADL2_Adapter_PMLog_Support_Get");
	ADL2_Adapter_PMLog_Support_Start = (ADL2_ADAPTER_PMLOG_SUPPORT_START)GetProcAddress(hDLL, "ADL2_Adapter_PMLog_Start");
	ADL2_Adapter_PMLog_Support_Stop = (ADL2_ADAPTER_PMLOG_SUPPORT_STOP)GetProcAddress(hDLL, "ADL2_Adapter_PMLog_Stop");
	ADL2_Desktop_Device_Create = (ADL2_DESKTOP_DEVICE_CREATE)GetProcAddress(hDLL, "ADL2_Desktop_Device_Create");
	ADL2_Desktop_Device_Destroy = (ADL2_DESKTOP_DEVICE_DESTROY)GetProcAddress(hDLL, "ADL2_Desktop_Device_Destroy");
 
    if (NULL == ADL2_Main_Control_Create ||
        NULL == ADL2_Main_Control_Destroy ||
        NULL == ADL2_Adapter_NumberOfAdapters_Get ||
        NULL == ADL2_Adapter_AdapterInfo_Get ||
        NULL == ADL2_Adapter_Active_Get ||
        NULL == ADL2_Overdrive_Caps ||
        NULL == ADL2_Adapter_RegValueInt_Get ||
        NULL == ADL2_Overdrive8_Init_Setting_Get ||
        NULL == ADL2_Overdrive8_Current_Setting_Get ||
        NULL == ADL2_Overdrive8_Setting_Set ||
        NULL == ADL2_New_QueryPMLogData_Get ||
        NULL == ADL2_Overdrive8_Init_SettingX2_Get ||
        NULL == ADL2_Overdrive8_Current_SettingX2_Get ||
		NULL == ADL2_Adapter_PMLog_Support_Get ||
		NULL == ADL2_Adapter_PMLog_Support_Start ||
		NULL == ADL2_Adapter_PMLog_Support_Stop ||
		NULL == ADL2_Desktop_Device_Create ||
		NULL == ADL2_Desktop_Device_Destroy)
    {
        PRINTF("Failed to get ADL function pointers\n");
        return FALSE;
    }
    
    if (ADL_OK != ADL2_Main_Control_Create(ADL_Main_Memory_Alloc, 1, &context))
    {
        printf("Failed to initialize nested ADL2 context");
        return ADL_ERR;
    }
    
    return TRUE;
}

void deinitializeADL()
{
    ADL2_Main_Control_Destroy(context);
    if (NULL != hDLL)
    {
        FreeLibrary(hDLL);
        hDLL = NULL;
    }
}

bool GetOD8OneRange(ADLOD8InitSetting initSettings, int featureID_)
{
    bool RangeSupport_ = (initSettings.overdrive8Capabilities & featureID_) ? true : false;
    return RangeSupport_;
}

int GetOD8InitSetting(int iAdapterIndex, ADLOD8InitSetting &odInitSetting)
{
    int ret = -1;
    memset(&odInitSetting, 0, sizeof(ADLOD8InitSetting));
    odInitSetting.count = OD8_COUNT;
    int overdrive8Capabilities;
    int numberOfFeatures = OD8_COUNT;
    ADLOD8SingleInitSetting* lpInitSettingList = NULL;
    if (NULL != ADL2_Overdrive8_Init_SettingX2_Get)
    {
        if (ADL_OK == ADL2_Overdrive8_Init_SettingX2_Get(context, iAdapterIndex, &overdrive8Capabilities, &numberOfFeatures, &lpInitSettingList))
        {
            odInitSetting.count = numberOfFeatures > OD8_COUNT ? OD8_COUNT : numberOfFeatures;
            odInitSetting.overdrive8Capabilities = overdrive8Capabilities;
            for (int i = 0; i < odInitSetting.count; i++)
            {
                odInitSetting.od8SettingTable[i].defaultValue = lpInitSettingList[i].defaultValue;
                odInitSetting.od8SettingTable[i].featureID = lpInitSettingList[i].featureID;
                odInitSetting.od8SettingTable[i].maxValue = lpInitSettingList[i].maxValue;
                odInitSetting.od8SettingTable[i].minValue = lpInitSettingList[i].minValue;
            }
            ADL_Main_Memory_Free((void**)&lpInitSettingList);
        }
        else
        {
            PRINTF("ADL2_Overdrive8_Init_SettingX2_Get is failed\n");
            ADL_Main_Memory_Free((void**)&lpInitSettingList);
            return ADL_ERR;
        }
    }
    else
    {
        if (NULL != ADL2_Overdrive8_Init_Setting_Get)
        {
            if (ADL_OK != ADL2_Overdrive8_Init_Setting_Get(context, iAdapterIndex, &odInitSetting))
            {
                PRINTF("ADL2_Overdrive8_Init_Setting_Get is failed\n");
                return ADL_ERR;
            }
        }
    }
    return ADL_OK;
}

int GetOD8CurrentSetting(int iAdapterIndex, ADLOD8CurrentSetting &odCurrentSetting)
{
    int ret = -1;
    memset(&odCurrentSetting, 0, sizeof(ADLOD8CurrentSetting));
    odCurrentSetting.count = OD8_COUNT;

    int numberOfFeaturesCurrent = OD8_COUNT;
    int* lpCurrentSettingList = NULL;
    if (NULL != ADL2_Overdrive8_Current_SettingX2_Get)
    {
        ret = ADL2_Overdrive8_Current_SettingX2_Get(context, iAdapterIndex, &numberOfFeaturesCurrent, &lpCurrentSettingList);
        if (0 == ret)
        {
            ret = -1;
            odCurrentSetting.count = numberOfFeaturesCurrent > OD8_COUNT ? OD8_COUNT : numberOfFeaturesCurrent;
            for (int i = 0; i < odCurrentSetting.count; i++)
            {
                odCurrentSetting.Od8SettingTable[i] = lpCurrentSettingList[i];
            }
            ADL_Main_Memory_Free((void**)&lpCurrentSettingList);
        }
        else
        {
            PRINTF("ADL2_Overdrive8_Current_SettingX2_Get is failed\n");
            ADL_Main_Memory_Free((void**)&lpCurrentSettingList);
            return ADL_ERR;
        }
    }
    else
    {
        if (NULL != ADL2_Overdrive8_Current_Setting_Get)
        {
            ret = ADL2_Overdrive8_Current_Setting_Get(context, iAdapterIndex, &odCurrentSetting);
            if (0 == ret)
                ret = -1;
            else
            {
                PRINTF("ADL2_Overdrive8_Current_Setting_Get is failed\n");
                return ADL_ERR;
            }
        }

    }
    return ADL_OK;
}
int printOD8GPUClocksParameters()
{
    int i;
    int ret = -1;
    int iSupported = 0, iEnabled = 0, iVersion = 0;

    // Repeat for all available adapters in the system
    for (i = 0; i < iNumberAdapters; i++)
    {
        PRINTF("-----------------------------------------\n");
        PRINTF("Adapter Index[%d]\n ", lpAdapterInfo[i].iAdapterIndex);
        PRINTF("-----------------------------------------\n");
        if (lpAdapterInfo[i].iBusNumber > -1)
        {
            ADL2_Overdrive_Caps(context, lpAdapterInfo[i].iAdapterIndex, &iSupported, &iEnabled, &iVersion);
            if (iVersion == 8)
            {
                //OD8 initial Status
                ADLOD8InitSetting odInitSetting;
                if (ADL_OK != GetOD8InitSetting(lpAdapterInfo[i].iAdapterIndex, odInitSetting))
                {
                    PRINTF("Get Init Setting failed.\n");
                    return ADL_ERR;
                }

                //OD8 Current Status
                ADLOD8CurrentSetting odCurrentSetting;
                if (ADL_OK != GetOD8CurrentSetting(lpAdapterInfo[i].iAdapterIndex, odCurrentSetting))
                {
                    PRINTF("Get Current Setting failed.\n");
                    return ADL_ERR;
                }
                
                ADLPMLogDataOutput odlpDataOutput;
                memset(&odlpDataOutput, 0, sizeof(ADLPMLogDataOutput));
                ret = ADL2_New_QueryPMLogData_Get(context, lpAdapterInfo[i].iAdapterIndex, &odlpDataOutput);
                if (0 == ret)
                {
                    if ((odInitSetting.overdrive8Capabilities & ADL_OD8_GFXCLK_LIMITS) == ADL_OD8_GFXCLK_LIMITS &&
                        (odInitSetting.overdrive8Capabilities & ADL_OD8_GFXCLK_CURVE) == ADL_OD8_GFXCLK_CURVE)
                    {
                        //GPU clocks
                        //OverdriveRangeDataStruct oneRangeData;
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FREQ1, ADL_OD8_GFXCLK_CURVE);
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FREQ2, ADL_OD8_GFXCLK_CURVE);
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FREQ3, ADL_OD8_GFXCLK_CURVE);
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FMIN, ADL_OD8_GFXCLK_CURVE);
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FMAX, ADL_OD8_GFXCLK_CURVE);
                        
                        PRINTF("ADLSensorType: PMLOG_CLK_GFXCLK\n");
                        PRINTF("PMLOG_CLK_GFXCLK.supported:%d\n", odlpDataOutput.sensors[PMLOG_CLK_GFXCLK].supported);
                        PRINTF("PMLOG_CLK_GFXCLK.value:%d\n", odlpDataOutput.sensors[PMLOG_CLK_GFXCLK].value);
                        PRINTF("-----------------------------------------\n");
                        PRINTF("ADLSensorType: PMLOG_INFO_ACTIVITY_GFX-GPU activity percentage value\n");
                        PRINTF("PMLOG_INFO_ACTIVITY_GFX.supported:%d\n", odlpDataOutput.sensors[PMLOG_INFO_ACTIVITY_GFX].supported);
                        PRINTF("PMLOG_INFO_ACTIVITY_GFX.value:%d\n", odlpDataOutput.sensors[PMLOG_INFO_ACTIVITY_GFX].value);
                        PRINTF("-----------------------------------------\n");

                    }
                    else if ((odInitSetting.overdrive8Capabilities & ADL_OD8_GFXCLK_LIMITS) == ADL_OD8_GFXCLK_LIMITS &&
                        (odInitSetting.overdrive8Capabilities & ADL_OD8_GFXCLK_CURVE) != ADL_OD8_GFXCLK_CURVE) {
                        //GPU clocks
                        OverdriveRangeDataStruct oneRangeData;
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FMIN, ADL_OD8_GFXCLK_CURVE);
                        //GetOD8RangePrint(odInitSetting, odCurrentSetting, oneRangeData, OD8_GFXCLK_FMAX, ADL_OD8_GFXCLK_CURVE);

                        PRINTF("ADLSensorType: PMLOG_CLK_GFXCLK\n");
                        PRINTF("PMLOG_CLK_GFXCLK.supported:%d\n", odlpDataOutput.sensors[PMLOG_CLK_GFXCLK].supported);
                        PRINTF("PMLOG_CLK_GFXCLK.value:%d\n", odlpDataOutput.sensors[PMLOG_CLK_GFXCLK].value);
                        PRINTF("-----------------------------------------\n");
                        PRINTF("ADLSensorType: PMLOG_INFO_ACTIVITY_GFX-GPU activity percentage value\n");
                        PRINTF("PMLOG_INFO_ACTIVITY_GFX.supported:%d\n", odlpDataOutput.sensors[PMLOG_INFO_ACTIVITY_GFX].supported);
                        PRINTF("PMLOG_INFO_ACTIVITY_GFX.value:%d\n", odlpDataOutput.sensors[PMLOG_INFO_ACTIVITY_GFX].value);
                        PRINTF("-----------------------------------------\n");
                    }
                    else
                        PRINTF("OD8PLUS Failed to get GPU clocks\n");
                }
                else
                {
                    PRINTF("ADL2_New_QueryPMLogData_Get is failed\n");
                    return ADL_ERR;
                }
                //break;
            }
        }

    }
    return 0;
}

std::string GetMachineName() {

    std::ifstream infile("nameconfig.txt");

    if (infile.good())
    {
        std::string sLine;
        std::getline(infile, sLine);
        PRINTF("the name of the machine is #s\n" , sLine);

        return sLine;
    }
    else
    {

        return "";
    }



}


int main(int argc, char* argv[])
{
    //read file from machine as the machine name
    std::string machinename = GetMachineName();
    if (machinename == "")
    {
        return 0;
    }


    if (initializeADL())
    {
            // Obtain the number of adapters for the system
            if (ADL_OK != ADL2_Adapter_NumberOfAdapters_Get(context, &iNumberAdapters))
            {
                PRINTF("Cannot get the number of adapters!\n");
                return 0;
            }
            if (0 < iNumberAdapters)
            {
                lpAdapterInfo = (LPAdapterInfo)malloc(sizeof(AdapterInfo)* iNumberAdapters);
                memset(lpAdapterInfo, '\0', sizeof(AdapterInfo)* iNumberAdapters);
                // Get the AdapterInfo structure for all adapters in the system
                ADL2_Adapter_AdapterInfo_Get(context, lpAdapterInfo, sizeof(AdapterInfo)* iNumberAdapters);
            }


            printOD8GPUClocksParameters();
 
        
            ADL_Main_Memory_Free((void**)&lpAdapterInfo);
            
            deinitializeADL();
    }

    return 0;
}