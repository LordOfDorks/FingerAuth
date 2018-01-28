// FingerAuth.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"

HANDLE hFPR = INVALID_HANDLE_VALUE;

bool InitializeFPR(
    LPCWSTR port)
{
    FPR_ERROR_CODE result;
    if (hFPR == INVALID_HANDLE_VALUE)
    {
        if ((hFPR = CreateFileW(port, GENERIC_READ || GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL)) == INVALID_HANDLE_VALUE)
        {
            return false;
        }
        if (!PurgeComm(hFPR, PURGE_TXABORT | PURGE_RXABORT | PURGE_TXCLEAR | PURGE_RXCLEAR))
        {
            return false;
        }
        DCB dcb;
        memset(&dcb, 0, sizeof(DCB));
        dcb.DCBlength = sizeof(DCB);
        dcb.BaudRate = CBR_9600;
        dcb.fBinary = TRUE;
        dcb.fParity = FALSE;
        dcb.ByteSize = 8;
        dcb.Parity = NOPARITY;
        dcb.StopBits = ONESTOPBIT;
        dcb.fAbortOnError = TRUE;
        if (!SetCommState(hFPR, &dcb))
        {
            return false;
        }
        COMMTIMEOUTS to;
        to.ReadIntervalTimeout = 0;
        to.ReadTotalTimeoutMultiplier = 0;
        to.ReadTotalTimeoutConstant = 16;
        to.WriteTotalTimeoutMultiplier = 0;
        to.WriteTotalTimeoutConstant = 16;
        if (!SetCommTimeouts(hFPR, &to))
        {
            return false;
        }
        result = FPR_ChangeBaudrate(115200);
        if ((result != FPR_ERROR_ACK_SUCCESS) && (result != FPR_ERROR_NACK_TIMEOUT))
        {
            return false;
        }
        dcb.BaudRate = CBR_115200;
        if (!SetCommState(hFPR, &dcb))
        {
            return false;
        }
        FPR_CMOSLEDControl(0);
    }
    return true;
}

unsigned char ReadCharFPR(int* timeout)
{
    unsigned int startTime = GetTickCount();
    unsigned char data = 0;
    unsigned int read = 0;
    COMMTIMEOUTS to;
    to.ReadIntervalTimeout = 0;
    to.ReadTotalTimeoutMultiplier = 0;
    to.ReadTotalTimeoutConstant = (unsigned int)*timeout;
    to.WriteTotalTimeoutMultiplier = 0;
    to.WriteTotalTimeoutConstant = 16;
    if (!SetCommTimeouts(hFPR, &to))
    {
        return false;
    }
    if (!ReadFile(hFPR, &data, 1, (LPDWORD)&read, NULL))
    {
        goto Cleanup;
    }

Cleanup:
    *timeout -= GetTickCount() - startTime;
    return data;
}

void WriteCharFPR(unsigned char data, int* timeout)
{
    unsigned int startTime = GetTickCount();
    TransmitCommChar(hFPR, data);
    *timeout -= GetTickCount() - startTime;
}

void CloseFPR()
{
    CloseHandle(hFPR);
    hFPR = INVALID_HANDLE_VALUE;
}

void SleepFPR(unsigned int duration)
{
    Sleep(duration);
}

int main()
{
    DEV_INFO_FPR devInfo = { 0 };
    unsigned int count = 0;
    unsigned int identify = 0;
    unsigned char fpTemplate[FP_TEMPLATE_SIZE] = { 0 };
    FPR_ERROR_CODE result;
    FPR_STATE_MACHINE state = FPR_STATE_MACHINE_START;

    if (!InitializeFPR(L"COM5"))
    {
        goto Cleanup;
    }
    if ((result = FPR_Open(&devInfo)) != FPR_ERROR_ACK_SUCCESS)
    {
        printf("FPR_Open failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    printf("FirmwareVersion = %08x, Serial = 0x", devInfo.FirmwareVersion);
    for (unsigned int n = 0; n < sizeof(devInfo.SerialNumber); n++)
    {
        printf("%02x", devInfo.SerialNumber[n]);
    }
    printf("\n");
    if ((result = FPR_GetEnrollCount(&count)) != FPR_ERROR_ACK_SUCCESS)
    {
        printf("FPR_GetEnrollCount failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    printf("EnrolledPrints: %d\n", count);

    FPR_DeleteId(0);
    FPR_DeleteId(1);
    printf("Enroll...\n");
    state = FPR_STATE_MACHINE_START;
    do
    {
        result = FPR_EnrollFinger(&state, 0, 0, 0, 0);
        if (result != FPR_ERROR_ACK_SUCCESS)
        {
            printf("FPR_EnrollFinger failed with 0x%08x.\n", result);
            goto Cleanup;
        }
        if (state != FPR_STATE_MACHINE_END)
        {
            Sleep(1);
        }
    } while (state != FPR_STATE_MACHINE_END);
    printf("Print [0] enrolled.\n");
    FPR_WaitForFinger(0, 30000);
    FPR_CMOSLEDControl(0);
    Sleep(2000);

    if((result = FPR_GetTemplate(0, fpTemplate)))
    {
        printf("FPR_GetTemplate failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    FPR_DeleteId(0);
    if ((result = FPR_SetTemplate(1, fpTemplate)))
    {
        printf("FPR_SetTemplate failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    printf("Template moved to [0]->[1]\n");
    if ((result = FPR_VerifyTemplate(1, fpTemplate)))
    {
        printf("FPR_VerifyTemplate failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    printf("Template [0] verified.\n");
    if ((result = FPR_IdentifyTemplate(&identify, fpTemplate)))
    {
        printf("FPR_IdentifyTemplate failed with 0x%08x.\n", result);
        goto Cleanup;
    }
    printf("Template [%d] identified.\n", identify);

    printf("Finger Verify...\n");
    state = FPR_STATE_MACHINE_START;
    do
    {
        result = FPR_VerifyFinger(&state, 1);
        if ((result != FPR_ERROR_ACK_SUCCESS) && (result != FPR_ERROR_NACK_VERIFY_FAILED))
        {
            printf("FPR_VerifyFinger failed with 0x%08x.\n", result);
            goto Cleanup;
        }
        if (state != FPR_STATE_MACHINE_END)
        {
            Sleep(1);
        }
    } while (state != FPR_STATE_MACHINE_END);
    printf("Print [0] %s.\n", (result == FPR_ERROR_ACK_SUCCESS) ? "Verified" : "NotVerified");
    FPR_WaitForFinger(0, 30000);
    FPR_CMOSLEDControl(0);
    Sleep(2000);

    printf("Finger Identify...\n");
    state = FPR_STATE_MACHINE_START;
    identify = 0;
    do
    {
        result = FPR_IdentifyFinger(&state, &identify);
        if ((result != FPR_ERROR_ACK_SUCCESS) && (result != FPR_ERROR_NACK_VERIFY_FAILED))
        {
            printf("FPR_IdentifyFinger failed with 0x%08x.\n", result);
            goto Cleanup;
        }
        if (state != FPR_STATE_MACHINE_END)
        {
            Sleep(1);
        }
    } while (state != FPR_STATE_MACHINE_END);
    printf("Print [%d] identified.\n", identify);

Cleanup:
    FPR_CMOSLEDControl(0);
    FPR_Close();
    CloseFPR();
    return 0;
}

