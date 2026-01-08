/**********************************************************************/
/*                                                                    */
/* Programmable Logic Controller for ESP microcontrollers             */
/*                                                                    */
/* Copyright (C) 2026 Denisov Foundation Limited                      */
/* License: GPLv3                                                     */
/* Written by Sergey Denisov aka LittleBuster                         */
/* Email: DenisovFoundationLtd@gmail.com                              */
/*                                                                    */
/**********************************************************************/

#pragma once

#include <Arduino.h>
#include <stdint.h>

class Sim800l
{
public:
    bool begin(Stream &ser)
    {
        _ser = &ser;
        return sync_();
    }

    bool sync(uint32_t timeout_ms = 1000) { return sendCommand_("AT", "OK", timeout_ms); }
    bool setEcho(bool on) { return sendCommand_(on ? "ATE1" : "ATE0"); }
    bool setSmsTextMode() { return sendCommand_("AT+CMGF=1"); }
    bool setCallerId(bool on) { return sendCommand_(on ? "AT+CLIP=1" : "AT+CLIP=0"); }

    bool getImei(String &out)
    {
        if (!sendCommand_("AT+GSN"))
            return false;
        return parseFirstNumberLine_(_last, out);
    }

    bool getImsi(String &out)
    {
        if (!sendCommand_("AT+CIMI"))
            return false;
        return parseFirstNumberLine_(_last, out);
    }

    bool getOperator(String &out)
    {
        if (!sendCommand_("AT+COPS?"))
            return false;
        String line;
        if (!extractLine_(_last, "+COPS:", line))
            return false;
        int q1 = line.indexOf('\"');
        int q2 = line.indexOf('\"', q1 + 1);
        if (q1 < 0 || q2 < 0)
            return false;
        out = line.substring(q1 + 1, q2);
        return true;
    }

    bool getSignalRssi(int &rssi)
    {
        if (!sendCommand_("AT+CSQ"))
            return false;
        String line;
        if (!extractLine_(_last, "+CSQ:", line))
            return false;
        int comma = line.indexOf(',');
        if (comma < 0)
            return false;
        rssi = line.substring(line.indexOf(':') + 1, comma).toInt();
        return true;
    }

    bool getRegStatus(int &stat)
    {
        if (!sendCommand_("AT+CREG?"))
            return false;
        String line;
        if (!extractLine_(_last, "+CREG:", line))
            return false;
        int comma = line.indexOf(',');
        if (comma < 0)
            return false;
        stat = line.substring(comma + 1).toInt();
        return true;
    }

    bool sendUssd(const String &code, String &response)
    {
        if (!sendCommand_(String("AT+CUSD=1,\"") + code + "\",15", "+CUSD:", 10000))
            return false;
        String line;
        if (!extractLine_(_last, "+CUSD:", line))
            return false;
        int q1 = line.indexOf('\"');
        int q2 = line.indexOf('\"', q1 + 1);
        if (q1 < 0 || q2 < 0)
            return false;
        response = line.substring(q1 + 1, q2);
        return true;
    }

    bool sendSMS(const String &number, const String &text)
    {
        if (!sendCommand_(String("AT+CMGS=\"") + number + "\"", ">", 2000))
            return false;
        _ser->print(text);
        _ser->write((uint8_t)0x1A);
        return waitFor_("OK", 10000);
    }

    bool dial(const String &number) { return sendCommand_(String("ATD") + number + ";"); }
    bool answer() { return sendCommand_("ATA"); }
    bool hangup() { return sendCommand_("ATH"); }

    bool listSms(String &out)
    {
        if (!sendCommand_("AT+CMGL=\"ALL\"", "OK", 10000))
            return false;
        out = _last;
        return true;
    }

    bool readSms(uint16_t index, String &out)
    {
        if (!sendCommand_(String("AT+CMGR=") + index, "OK", 5000))
            return false;
        out = _last;
        return true;
    }

    bool deleteSms(uint16_t index) { return sendCommand_(String("AT+CMGD=") + index); }

    bool setApn(const String &apn, const String &user = "", const String &pass = "")
    {
        if (!sendCommand_("AT+SAPBR=3,1,\"CONTYPE\",\"GPRS\""))
            return false;
        if (!sendCommand_(String("AT+SAPBR=3,1,\"APN\",\"") + apn + "\""))
            return false;
        if (user.length())
        {
            if (!sendCommand_(String("AT+SAPBR=3,1,\"USER\",\"") + user + "\""))
                return false;
        }
        if (pass.length())
        {
            if (!sendCommand_(String("AT+SAPBR=3,1,\"PWD\",\"") + pass + "\""))
                return false;
        }
        return true;
    }

    bool openBearer() { return sendCommand_("AT+SAPBR=1,1", "OK", 10000); }
    bool closeBearer() { return sendCommand_("AT+SAPBR=0,1", "OK", 10000); }

    bool getBearerIp(String &out)
    {
        if (!sendCommand_("AT+SAPBR=2,1"))
            return false;
        String line;
        if (!extractLine_(_last, "+SAPBR:", line))
            return false;
        int q1 = line.indexOf('\"');
        int q2 = line.indexOf('\"', q1 + 1);
        if (q1 < 0 || q2 < 0)
            return false;
        out = line.substring(q1 + 1, q2);
        return true;
    }

    bool httpInit() { return sendCommand_("AT+HTTPINIT"); }
    bool httpTerm() { return sendCommand_("AT+HTTPTERM"); }
    bool httpSetCid(uint8_t cid = 1) { return sendCommand_(String("AT+HTTPPARA=\"CID\",") + cid); }
    bool httpSetUrl(const String &url) { return sendCommand_(String("AT+HTTPPARA=\"URL\",\"") + url + "\""); }
    bool httpSetContentType(const String &type) { return sendCommand_(String("AT+HTTPPARA=\"CONTENT\",\"") + type + "\""); }

    bool httpGet(int &status, int &len)
    {
        if (!sendCommand_("AT+HTTPACTION=0", "+HTTPACTION:", 10000))
            return false;
        String line = readLine_(2000);
        int s = -1, l = -1;
        if (!parseHttpAction_(line, s, l))
            return false;
        status = s;
        len = l;
        return true;
    }

    bool httpRead(String &out)
    {
        if (!sendCommand_("AT+HTTPREAD", "+HTTPREAD:", 2000))
            return false;
        String header = readLine_(2000);
        int len = parseHttpReadLen_(header);
        if (len <= 0)
            return false;
        out = readBytes_(len, 2000);
        waitFor_("OK", 2000);
        return true;
    }

    bool httpPost(const String &url, const String &content_type, const String &data, int &status, int &len)
    {
        if (!httpInit())
            return false;
        if (!httpSetCid())
            return false;
        if (!httpSetUrl(url))
            return false;
        if (!httpSetContentType(content_type))
            return false;
        if (!sendCommand_(String("AT+HTTPDATA=") + data.length() + ",5000", "DOWNLOAD", 5000))
            return false;
        _ser->print(data);
        if (!waitFor_("OK", 5000))
            return false;
        if (!sendCommand_("AT+HTTPACTION=1", "+HTTPACTION:", 10000))
            return false;
        String line;
        if (!extractLine_(_last, "+HTTPACTION:", line))
            return false;
        if (!parseHttpAction_(line, status, len))
            return false;
        return true;
    }

    bool powerDown() { return sendCommand_("AT+CPOWD=1", "POWER DOWN", 5000); }

    bool readUrc(String &out)
    {
        out = "";
        if (!_ser || !_ser->available())
            return false;
        String line = readLine_(100);
        if (!line.length())
            return false;
        if (line.startsWith("RING") ||
            line.startsWith("+CLIP:") ||
            line.startsWith("+CMTI:") ||
            line.startsWith("+SAPBR:") ||
            line.startsWith("+HTTPACTION:"))
        {
            out = line;
            return true;
        }
        return false;
    }

    bool pollSmsIndex(uint16_t &out_index)
    {
        out_index = 0;
        if (!_ser || !_ser->available())
            return false;
        String line = readLine_(100);
        if (!line.startsWith("+CMTI:"))
            return false;
        int comma = line.indexOf(',');
        if (comma < 0)
            return false;
        out_index = (uint16_t)line.substring(comma + 1).toInt();
        return out_index > 0;
    }

    bool pollIncomingCall(String &out_number)
    {
        out_number = "";
        if (!_ser || !_ser->available())
            return false;
        String line = readLine_(100);
        if (line.startsWith("+CLIP:"))
        {
            int q1 = line.indexOf('\"');
            int q2 = line.indexOf('\"', q1 + 1);
            if (q1 < 0 || q2 < 0)
                return false;
            out_number = line.substring(q1 + 1, q2);
            return true;
        }
        return false;
    }

    bool pollIncomingCallRing(String &out_number, uint32_t wait_ms = 500)
    {
        out_number = "";
        if (!_ser)
            return false;
        String line = readLine_(100);
        if (!line.startsWith("RING"))
            return false;

        const uint32_t start = millis();
        while ((millis() - start) < wait_ms)
        {
            String l = readLine_(100);
            if (l.startsWith("+CLIP:"))
            {
                int q1 = l.indexOf('\"');
                int q2 = l.indexOf('\"', q1 + 1);
                if (q1 < 0 || q2 < 0)
                    return false;
                out_number = l.substring(q1 + 1, q2);
                return true;
            }
        }
        return false;
    }

    const String &lastResponse() const { return _last; }

    bool readCallerId(String &out_number)
    {
        out_number = "";
        if (!_ser)
            return false;

        while (_ser->available())
        {
            String line = readLine_(50);
            if (line.startsWith("+CLIP:"))
            {
                int q1 = line.indexOf('\"');
                if (q1 < 0)
                    continue;
                int q2 = line.indexOf('\"', q1 + 1);
                if (q2 < 0)
                    continue;
                out_number = line.substring(q1 + 1, q2);
                return true;
            }
        }
        return false;
    }

private:
    bool sync_() { return sendCommand_("AT"); }

    bool sendCommand_(const String &cmd, const char *expect = "OK", uint32_t timeout_ms = 1000)
    {
        if (!_ser)
            return false;
        flush_();
        _ser->print(cmd);
        _ser->print("\r");
        return waitFor_(expect, timeout_ms);
    }

    bool waitFor_(const char *token, uint32_t timeout_ms)
    {
        _last = "";
        const uint32_t start = millis();
        while ((millis() - start) < timeout_ms)
        {
            while (_ser && _ser->available())
            {
                char c = (char)_ser->read();
                _last += c;
                if (_last.endsWith(token))
                    return true;
            }
        }
        return false;
    }

    String readLine_(uint32_t timeout_ms)
    {
        String line;
        const uint32_t start = millis();
        while ((millis() - start) < timeout_ms)
        {
            while (_ser && _ser->available())
            {
                char c = (char)_ser->read();
                if (c == '\n')
                    return line;
                if (c != '\r')
                    line += c;
            }
        }
        return line;
    }

    String readBytes_(size_t len, uint32_t timeout_ms)
    {
        String out;
        out.reserve(len);
        const uint32_t start = millis();
        while ((millis() - start) < timeout_ms && out.length() < len)
        {
            while (_ser && _ser->available() && out.length() < len)
            {
                out += (char)_ser->read();
            }
        }
        return out;
    }

    void flush_()
    {
        while (_ser && _ser->available())
            (void)_ser->read();
    }

    static bool parseHttpAction_(const String &line, int &status, int &len)
    {
        int i1 = line.indexOf(',');
        int i2 = line.indexOf(',', i1 + 1);
        if (i1 < 0 || i2 < 0)
            return false;
        status = line.substring(i1 + 1, i2).toInt();
        len = line.substring(i2 + 1).toInt();
        return true;
    }

    static int parseHttpReadLen_(const String &line)
    {
        int i = line.indexOf(':');
        if (i < 0)
            return -1;
        return line.substring(i + 1).toInt();
    }

    static bool extractLine_(const String &src, const char *prefix, String &out)
    {
        int start = src.indexOf(prefix);
        if (start < 0)
            return false;
        int end = src.indexOf('\n', start);
        if (end < 0)
            end = src.length();
        out = src.substring(start, end);
        out.trim();
        return true;
    }

    static bool parseFirstNumberLine_(const String &src, String &out)
    {
        int start = 0;
        while (start < (int)src.length())
        {
            int end = src.indexOf('\n', start);
            if (end < 0)
                end = src.length();
            String line = src.substring(start, end);
            line.trim();
            if (line.length() >= 10 && isDigit(line[0]))
            {
                out = line;
                return true;
            }
            start = end + 1;
        }
        return false;
    }

    Stream *_ser = nullptr;
    String _last;
};
