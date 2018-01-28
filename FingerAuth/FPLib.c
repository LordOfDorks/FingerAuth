#include "FPLib.h"

COMMAND_INFO cmdInfo[] = {
{FPR_COMMAND_OPEN, 0, sizeof(DEV_INFO_FPR)},
{FPR_COMMAND_CLOSE, 0, 0},
{FPR_COMMAND_USBINTERNALCHECK, 0, 0},
{FPR_COMMAND_CHANGEBAUDRATE,0 ,0},
{FPR_COMMAND_SETIAPMODE, 0, 0},
{FPR_COMMAND_CMOSLED, 0, 0 },
{FPR_COMMAND_GETENROLLCOUNT, 0, 0 },
{FPR_COMMAND_CHECKENROLLED, 0, 0 },
{FPR_COMMAND_ENROLLSTART, 0, 0 },
{FPR_COMMAND_ENROLL1, 0, 0 },
{FPR_COMMAND_ENROLL2, 0, 0 },
{FPR_COMMAND_ENROLL3, 0, FP_TEMPLATE_SIZE },
{FPR_COMMAND_ISPRESSFINGER, 0, 0 },
{FPR_COMMAND_DELETEID, 0, 0 },
{FPR_COMMAND_DELETEALL, 0, 0 },
{FPR_COMMAND_VERIFY, 0, 0 },
{FPR_COMMAND_IDENTIFY, 0, 0 },
{FPR_COMMAND_VERIFYTEMPLATE, FP_TEMPLATE_SIZE, 0 },
{FPR_COMMAND_IDENTIFYTEMPLATE, FP_TEMPLATE_SIZE, 0 },
{FPR_COMMAND_CAPTUREFINGER, 0, 0 },
{FPR_COMMAND_MAKETEMPLATE, 0, FP_TEMPLATE_SIZE },
{FPR_COMMAND_GETIMAGE, 0, FP_IMAGE_SIZE },
{FPR_COMMAND_GETRAWIMAGE, 0, FP_RAW_IMAGE_SIZE },
{FPR_COMMAND_GETTEMPLATE, 0, FP_TEMPLATE_SIZE },
{FPR_COMMAND_SETTEMPLATE, FP_TEMPLATE_SIZE, 0 },
{FPR_COMMAND_GETDATABASESTART, 0, 0 },
{FPR_COMMAND_GETDATABASEEND, 0, 0 },
{FPR_COMMAND_SETSECURITYLEVEL, 0, 0 },
{FPR_COMMAND_GETSECURITYLEVEL, 0, 0 },
{FPR_COMMAND_INVALID, 0, 0}
};

static COMMAND_INFO* GetCommandInfo(
    FPR_COMMAND_CODE cmd)
{
    for (unsigned int n = 0; cmdInfo[n].cmd != FPR_COMMAND_INVALID; n++)
    {
        if (cmdInfo[n].cmd == cmd) return &cmdInfo[n];
    }
    return 0;
}

static unsigned int CheckSum(
    unsigned short sumIn,
    unsigned char* data,
    unsigned int len)
{
    unsigned short sum = sumIn;
    for (unsigned int n = 0; n < len; n++) sum += data[n];
    return sum;
}

static FPR_ERROR_CODE Execute(
    FPR_COMMAND_CODE cmd,
    unsigned int parameterIn,
    unsigned int* parameterOut,
    unsigned char* dataPktIn,
    unsigned int dataPktInSize,
    unsigned char* dataPktOut,
    unsigned int dataPktOutSize,
    int* timeout)
{
    FPR_ERROR_CODE result = FPR_ERROR_ACK_SUCCESS;
    COMMAND_INFO* info = GetCommandInfo(cmd);
    FPR_COMMAND_RESPONSE_PACKET_T in;
    FPR_COMMAND_RESPONSE_PACKET_T out;
    unsigned short chkSum = 0;

    // check valid command
    if (info == 0)
    {
        result = FPR_ERROR_NACK_INVALID_PARAM;
        goto Cleanup;
    }

    // Send the command out
    in.s.startCode1 = 0x55;
    in.s.startCode2 = 0xAA;
    in.s.deviceId = 0x0001;
    in.s.parameter = parameterIn;
    in.s.cmd_rsp = (unsigned short)cmd;
    in.s.checkSum = CheckSum(0, (unsigned char*)&in, sizeof(in) - sizeof(in.s.checkSum));
    for (unsigned int n = 0; n < sizeof(in); n++)
    {
        if (*timeout <= 0)
        {
            result = FPR_ERROR_NACK_TIMEOUT;
            goto Cleanup;
        }
        WriteCharFPR(in.raw[n], timeout);
    }

    // Retrive the response
    for (unsigned int n = 0; n < sizeof(out); n++)
    {
        if (*timeout <= 0)
        {
            result = FPR_ERROR_NACK_TIMEOUT;
            goto Cleanup;
        }
        out.raw[n] = ReadCharFPR(timeout);
    }
    if ((out.s.startCode1 != 0x55) ||
        (out.s.startCode2 != 0xAA) ||
        (out.s.deviceId != 0x0001) ||
        (out.s.checkSum != CheckSum(0, (unsigned char*)&out, sizeof(out) - sizeof(out.s.checkSum))))
    {
        result = FPR_ERROR_NACK_COMM_ERR;
        goto Cleanup;
    }
    if (out.s.cmd_rsp == (unsigned short)FPR_COMMAND_NACK)
    {
        result = out.s.parameter;
        goto Cleanup;
    }
    if (parameterOut != 0)
    {
        *parameterOut = out.s.parameter;
    }

    // Send Data Packet
    if (info->dataIn > 0)
    {
        if((dataPktInSize != info->dataIn) || (dataPktIn == 0))
        {
            result = FPR_ERROR_NACK_INVALID_PARAM;
            goto Cleanup;
        }
        in.s.startCode1 = 0x5A;
        in.s.startCode2 = 0xA5;
        in.s.deviceId = 0x0001;
        in.s.checkSum = CheckSum(0, (unsigned char*)&in, sizeof(in.s.startCode1) + sizeof(in.s.startCode2) + sizeof(in.s.deviceId));
        in.s.checkSum = CheckSum(in.s.checkSum, dataPktIn, dataPktInSize);
        for (unsigned int n = 0; n < sizeof(in.s.startCode1) + sizeof(in.s.startCode2) + sizeof(in.s.deviceId); n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            WriteCharFPR(in.raw[n], timeout);
        }
        for (unsigned int n = 0; n < dataPktInSize; n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            WriteCharFPR(dataPktIn[n], timeout);
        }
        for (unsigned int n = sizeof(in) - sizeof(in.s.checkSum); n < sizeof(in); n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            WriteCharFPR(in.raw[n], timeout);
        }

        // Retrive the data packet response
        for (unsigned int n = 0; n < sizeof(out); n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            out.raw[n] = ReadCharFPR(timeout);
        }
        if ((out.s.startCode1 != 0x55) ||
            (out.s.startCode2 != 0xAA) ||
            (out.s.deviceId != 0x0001) ||
            (out.s.checkSum != CheckSum(0, (unsigned char*)&out, sizeof(out) - sizeof(out.s.checkSum))))
        {
            result = FPR_ERROR_NACK_COMM_ERR;
            goto Cleanup;
        }
        if (out.s.cmd_rsp == (unsigned short)FPR_COMMAND_NACK)
        {
            result = out.s.parameter;
            goto Cleanup;
        }
        if (parameterOut != 0)
        {
            *parameterOut = out.s.parameter;
        }
    }

    // Recive Data Packet
    if ((info->dataOut > 0) && (dataPktOut > 0) && (dataPktOut != 0))
    {
        for (unsigned int n = 0; n < sizeof(out.s.startCode1) + sizeof(out.s.startCode2) + sizeof(out.s.deviceId); n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            out.raw[n] = ReadCharFPR(timeout);
        }
        for (unsigned int n = 0; n < dataPktOutSize; n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            dataPktOut[n] = ReadCharFPR(timeout);

        }
        for (unsigned int n = sizeof(out) - sizeof(out.s.checkSum); n < sizeof(out); n++)
        {
            if (*timeout <= 0)
            {
                result = FPR_ERROR_NACK_TIMEOUT;
                goto Cleanup;
            }
            out.raw[n] = ReadCharFPR(timeout);
        }
        if ((out.s.startCode1 != 0x5A) ||
            (out.s.startCode2 != 0xA5) ||
            (out.s.deviceId != 0x0001) ||
            (out.s.checkSum != CheckSum(CheckSum(0, (unsigned char*)&out, sizeof(out.s.startCode1) + sizeof(out.s.startCode2) + sizeof(out.s.deviceId)), dataPktOut, dataPktOutSize)))
        {
            result = FPR_ERROR_NACK_COMM_ERR;
            goto Cleanup;
        }
    }

Cleanup:
    return result;
}

FPR_ERROR_CODE FPR_Open(DEV_INFO_FPR* info)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_OPEN, (info ? 1 : 0), 0, 0, 0, (unsigned char*)info, sizeof(DEV_INFO_FPR), &timeout);
}

FPR_ERROR_CODE FPR_Close(void)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_CLOSE, 0, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_CMOSLEDControl(unsigned int on)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_CMOSLED, on, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_ChangeBaudrate(unsigned int baudrate)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_CHANGEBAUDRATE, baudrate, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_GetEnrollCount(unsigned int* count)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_GETENROLLCOUNT, 0, count, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_CheckEnrolled(unsigned int id)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_CHECKENROLLED, id, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_EnrollStart(unsigned int id, unsigned char noDupChk, unsigned char noSave)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_ENROLLSTART, (noSave ? -1 : (noDupChk ? 0x80000000 : 0) | (0x0000ffff & id)), 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_Enroll(unsigned int no, unsigned char* fpTemplate)
{
    int timeout = 1000;
    return Execute((no == 1 ? FPR_COMMAND_ENROLL1 : (no == 2 ? FPR_COMMAND_ENROLL2 : FPR_COMMAND_ENROLL3)), 0, 0, 0, 0, fpTemplate, (fpTemplate ? FP_TEMPLATE_SIZE : 0), &timeout);
}

FPR_ERROR_CODE FPR_IsPressFinger(void)
{
    unsigned int parameter;
    int timeout = FP_DEFAULT_TIMEOUT;
    FPR_ERROR_CODE result = Execute(FPR_COMMAND_ISPRESSFINGER, 0, &parameter, 0, 0, 0, 0, &timeout);
    return (result == FPR_ERROR_ACK_SUCCESS) ? parameter : result;
}

FPR_ERROR_CODE FPR_DeleteId(unsigned int id)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_DELETEID, id, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_DeleteAll(void)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_DELETEALL, 0, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_Verify(unsigned int id)
{
    int timeout = 1000;
    return Execute(FPR_COMMAND_VERIFY, id, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_Identify(unsigned int* id)
{
    int timeout = 1000;
    return Execute(FPR_COMMAND_IDENTIFY, 0, id, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_VerifyTemplate(unsigned int id, unsigned char* fpTemplate)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_VERIFYTEMPLATE, id, 0, fpTemplate, FP_TEMPLATE_SIZE, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_IdentifyTemplate(unsigned int* id, unsigned char* fpTemplate)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_IDENTIFYTEMPLATE, 0, id, fpTemplate, FP_TEMPLATE_SIZE, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_CaptureFinger(unsigned char bestImage)
{
    int timeout = 250;
    return Execute(FPR_COMMAND_CAPTUREFINGER, bestImage, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_MakeTemplate(unsigned char* fpTemplate)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_MAKETEMPLATE, 0, 0, 0, 0, fpTemplate, FP_TEMPLATE_SIZE, &timeout);
}

FPR_ERROR_CODE FPR_GetImage(unsigned char* fpImage)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_GETIMAGE, 0, 0, 0, 0, fpImage, FP_IMAGE_SIZE, &timeout);
}

FPR_ERROR_CODE FPR_GetRawImage(unsigned char* fpRawImage)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_GETRAWIMAGE, 0, 0, 0, 0, fpRawImage, FP_RAW_IMAGE_SIZE, &timeout);
}

FPR_ERROR_CODE FPR_GetTemplate(unsigned int id, unsigned char* fpTemplate)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_GETTEMPLATE, id, 0, 0, 0, fpTemplate, FP_TEMPLATE_SIZE, &timeout);
}

FPR_ERROR_CODE FPR_SetTemplate(unsigned int id, unsigned char* fpTemplate)
{
    int timeout = 1000;
    return Execute(FPR_COMMAND_SETTEMPLATE, id, 0, fpTemplate, FP_TEMPLATE_SIZE, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_SetIAPMode(void)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_SETIAPMODE, 0, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_SetSecurityLevel(unsigned int level)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_SETSECURITYLEVEL, level, 0, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_GetSecurityLevel(unsigned int* level)
{
    int timeout = FP_DEFAULT_TIMEOUT;
    return Execute(FPR_COMMAND_GETSECURITYLEVEL, 0, level, 0, 0, 0, 0, &timeout);
}

FPR_ERROR_CODE FPR_WaitForFinger(unsigned char pressed, unsigned int cycles)
{
    FPR_ERROR_CODE result = FPR_ERROR_ACK_SUCCESS;
    if ((result = FPR_CMOSLEDControl(1)) != FPR_ERROR_ACK_SUCCESS)
    {
        goto Cleanup;
    }
    do
    {
        if (cycles == 0)
        {
            result = FPR_ERROR_NACK_TIMEOUT;
            goto Cleanup;
        }
        result = FPR_IsPressFinger();
        if((result != FPR_ERROR_ACK_SUCCESS) && (result != FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED))
        {
            goto Cleanup;
        }
        if(cycles < 0xffffffff) cycles--;
    } while (((result == FPR_ERROR_ACK_SUCCESS) && (!pressed)) ||
             ((result == FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED) && (pressed)));
    result = FPR_ERROR_ACK_SUCCESS;
Cleanup:
    return result;
}

FPR_ERROR_CODE FPR_EnrollFinger(FPR_STATE_MACHINE* state, unsigned int id, unsigned char noDupChk, unsigned char noSave, unsigned char* fpTemplate)
{
    FPR_ERROR_CODE result = FPR_ERROR_ACK_SUCCESS;

    if (*state == FPR_STATE_MACHINE_START)
    {
        if ((result = FPR_EnrollStart(id, noDupChk, noSave)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        if ((result = FPR_CMOSLEDControl(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        *state = FPR_STATE_MACHINE_ENROLL_FIRST_PRESS;
    }

Restart:
    if ((*state == FPR_STATE_MACHINE_ENROLL_FIRST_PRESS) ||
        (*state == FPR_STATE_MACHINE_ENROLL_SECOND_PRESS) ||
        (*state == FPR_STATE_MACHINE_ENROLL_THIRD_PRESS))
    {
        result = FPR_IsPressFinger();
        if (result == FPR_ERROR_ACK_SUCCESS)
        {
            *state += 1;
        }
        else if (result == FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED)
        {
            result = FPR_ERROR_ACK_SUCCESS;
            goto Cleanup;
        }
        else
        {
            goto Cleanup;
        }
    }

    if ((*state == FPR_STATE_MACHINE_ENROLL_FIRST_SCAN) ||
        (*state == FPR_STATE_MACHINE_ENROLL_SECOND_SCAN) ||
        (*state == FPR_STATE_MACHINE_ENROLL_THIRD_SCAN))
    {
        unsigned int no = (*state == FPR_STATE_MACHINE_ENROLL_FIRST_SCAN ? 1 : (*state == FPR_STATE_MACHINE_ENROLL_SECOND_SCAN ? 2 : 3));

        if ((result = FPR_CaptureFinger(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        if ((result = FPR_Enroll(no, (*state == FPR_STATE_MACHINE_ENROLL_THIRD_SCAN ? fpTemplate : 0))) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        if ((result = FPR_CMOSLEDControl(0)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        if (*state == FPR_STATE_MACHINE_ENROLL_THIRD_SCAN)
        {
            if ((result = FPR_CMOSLEDControl(0)) != FPR_ERROR_ACK_SUCCESS)
            {
                goto Cleanup;
            }
            *state = FPR_STATE_MACHINE_END;
            goto Cleanup;
        }
        else
        {
            *state += 1;
        }
        goto Cleanup;
    }

    if (((*state >= FPR_STATE_MACHINE_ENROLL_FIRST_LIGHT_OFF) && (*state < FPR_STATE_MACHINE_ENROLL_FIRST_LIGHT_ON)) ||
        ((*state >= FPR_STATE_MACHINE_ENROLL_SECOND_LIGHT_OFF) && (*state < FPR_STATE_MACHINE_ENROLL_SECOND_LIGHT_ON)))
    {
        *state += 1;
        goto Cleanup;
    }

    if ((*state == FPR_STATE_MACHINE_ENROLL_FIRST_LIGHT_ON) ||
        (*state == FPR_STATE_MACHINE_ENROLL_SECOND_LIGHT_ON))
    {
        if ((result = FPR_CMOSLEDControl(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        *state += 1;
    }

    if ((*state == FPR_STATE_MACHINE_ENROLL_FIRST_REMOVED) ||
        (*state == FPR_STATE_MACHINE_ENROLL_SECOND_REMOVED))
    {
        result = FPR_IsPressFinger();
        if (result == FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED)
        {
            result = FPR_ERROR_ACK_SUCCESS;
            *state += 1;
            goto Restart;
        }
        else
        {
            goto Cleanup;
        }
    }

Cleanup:
    return result;
}

FPR_ERROR_CODE FPR_VerifyFinger(FPR_STATE_MACHINE* state, unsigned int id)
{
    FPR_ERROR_CODE result = FPR_ERROR_ACK_SUCCESS;

    if (*state == FPR_STATE_MACHINE_START)
    {
        if ((result = FPR_CMOSLEDControl(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        *state = FPR_STATE_MACHINE_VERIFY_PRESS;
    }

    if (*state == FPR_STATE_MACHINE_VERIFY_PRESS)
    {
        result = FPR_IsPressFinger();
        if (result == FPR_ERROR_ACK_SUCCESS)
        {
            *state += 1;
        }
        else if (result == FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED)
        {
            result = FPR_ERROR_ACK_SUCCESS;
            goto Cleanup;
        }
        else
        {
            goto Cleanup;
        }
    }

    if (*state == FPR_STATE_MACHINE_VERIFY_SCAN)
    {
        if ((result = FPR_CaptureFinger(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        result = FPR_Verify(id);
        FPR_CMOSLEDControl(0);
        if (result != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        
        *state = FPR_STATE_MACHINE_END;
        goto Cleanup;
    }

Cleanup:
    return result;
}

FPR_ERROR_CODE FPR_IdentifyFinger(FPR_STATE_MACHINE* state, unsigned int* id)
{
    FPR_ERROR_CODE result = FPR_ERROR_ACK_SUCCESS;

    if (*state == FPR_STATE_MACHINE_START)
    {
        if ((result = FPR_CMOSLEDControl(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        *state = FPR_STATE_MACHINE_VERIFY_PRESS;
    }

    if (*state == FPR_STATE_MACHINE_VERIFY_PRESS)
    {
        result = FPR_IsPressFinger();
        if (result == FPR_ERROR_ACK_SUCCESS)
        {
            *state += 1;
        }
        else if (result == FPR_ERROR_NACK_FINGER_IS_NOT_PRESSED)
        {
            result = FPR_ERROR_ACK_SUCCESS;
            goto Cleanup;
        }
        else
        {
            goto Cleanup;
        }
    }

    if (*state == FPR_STATE_MACHINE_VERIFY_SCAN)
    {
        if ((result = FPR_CaptureFinger(1)) != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }
        result = FPR_Identify(id);
        FPR_CMOSLEDControl(0);
        if (result != FPR_ERROR_ACK_SUCCESS)
        {
            goto Cleanup;
        }

        *state = FPR_STATE_MACHINE_END;
        goto Cleanup;
    }

Cleanup:
    return result;
}
