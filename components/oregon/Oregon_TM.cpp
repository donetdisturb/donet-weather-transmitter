#include "Oregon_TM.h"

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// This file is part of the Arduino OREGON_NR library.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// The MIT License (MIT)
//
// Copyright (c) 2021 Sergey Zawislak
//
// Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"),
// to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense,
// and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//  Этот файл - часть библиотеки OREGON_NR
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//
// Copyright (c) 2021 Сергей Зависляк
//
// Данная лицензия разрешает лицам, получившим копию данного программного обеспечения и сопутствующей документации
// (в дальнейшем именуемыми «Программное Обеспечение»), безвозмездно использовать Программное Обеспечение без ограничений,
// включая неограниченное право на использование, копирование, изменение, слияние, публикацию, распространение, сублицензирование
// и/или продажу копий Программного Обеспечения, а также лицам, которым предоставляется данное Программное Обеспечение, при соблюдении следующих условий:
//
// Указанное выше уведомление об авторском праве и данные условия должны быть включены во все копии или значимые части данного Программного Обеспечения.
//
// ДАННОЕ ПРОГРАММНОЕ ОБЕСПЕЧЕНИЕ ПРЕДОСТАВЛЯЕТСЯ «КАК ЕСТЬ», БЕЗ КАКИХ-ЛИБО ГАРАНТИЙ, ЯВНО ВЫРАЖЕННЫХ ИЛИ ПОДРАЗУМЕВАЕМЫХ, ВКЛЮЧАЯ ГАРАНТИИ ТОВАРНОЙ
// ПРИГОДНОСТИ, СООТВЕТСТВИЯ ПО ЕГО КОНКРЕТНОМУ НАЗНАЧЕНИЮ И ОТСУТСТВИЯ НАРУШЕНИЙ, НО НЕ ОГРАНИЧИВАЯСЬ ИМИ. НИ В КАКОМ СЛУЧАЕ АВТОРЫ ИЛИ ПРАВООБЛАДАТЕЛИ
// НЕ НЕСУТ ОТВЕТСТВЕННОСТИ ПО КАКИМ-ЛИБО ИСКАМ, ЗА УЩЕРБ ИЛИ ПО ИНЫМ ТРЕБОВАНИЯМ, В ТОМ ЧИСЛЕ, ПРИ ДЕЙСТВИИ КОНТРАКТА, ДЕЛИКТЕ ИЛИ ИНОЙ СИТУАЦИИ,
// ВОЗНИКШИМ ИЗ-ЗА ИСПОЛЬЗОВАНИЯ ПРОГРАММНОГО ОБЕСПЕЧЕНИЯ ИЛИ ИНЫХ ДЕЙСТВИЙ С ПРОГРАММНЫМ ОБЕСПЕЧЕНИЕМ.
/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////




//Конструктор

Oregon_TM::Oregon_TM(byte tr_pin, int buf_size)
{
  max_buffer_size = (int)(buf_size / 2) + 2;
  SendBuffer = new byte[max_buffer_size + 2];
  memset(SendBuffer, 0, max_buffer_size + 2);
  TX_PIN = tr_pin;
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);

}

Oregon_TM::Oregon_TM(byte tr_pin)
{
  SendBuffer = new byte[max_buffer_size + 2];
  memset(SendBuffer, 0, max_buffer_size + 2);
  TX_PIN = tr_pin;
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);

}

Oregon_TM::Oregon_TM(void)
{
  SendBuffer = new byte[max_buffer_size + 2];
  memset(SendBuffer, 0, max_buffer_size + 2);
  pinMode(TX_PIN, OUTPUT);
  digitalWrite(TX_PIN, LOW);
}


///////////////////////////////////////////////////////////////////////////////////////////////////
// Функции передатчика////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////
void IRAM_ATTR Oregon_TM::sendZero(void)
{
  if (protocol == 2) {
    while (time_marker + TR_TIME * 4 >= micros());
    time_marker += TR_TIME * 4;
    digitalWrite(TX_PIN, HIGH);
    delayMicroseconds(TR_TIME - PULSE_SHORTEN_2);
    digitalWrite(TX_PIN, LOW);
    delayMicroseconds(TWOTR_TIME + PULSE_SHORTEN_2);
    digitalWrite(TX_PIN, HIGH);
  }
  if (protocol == 3)
  {
    if (prevstate) while (time_marker + TWOTR_TIME - PULSE_SHORTEN_3 >= micros());
    else while (time_marker + TWOTR_TIME >= micros());

    time_marker += TWOTR_TIME;

    if (prevbit && prevstate)
    {
      digitalWrite(TX_PIN, LOW);
      prevstate = 0;
      prevbit = 0;
      return;
    }
    if (prevbit && !prevstate)
    {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(TWOTR_TIME);
      prevstate = 1;
      prevbit = 0;
      return;
    }
    if (!prevbit && prevstate)
    {
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(TR_TIME);
      digitalWrite(TX_PIN, HIGH);
      prevbit = 0;
      return;
    }
    if (!prevbit && !prevstate)
    {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(TR_TIME);
      digitalWrite(TX_PIN, LOW);
      prevbit = 0;
      return;
    }
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendOne(void)
{
  if (protocol == 2) {
    while (time_marker + TR_TIME * 4 - PULSE_SHORTEN_2 >= micros());
    time_marker += TR_TIME * 4;
    digitalWrite(TX_PIN, LOW);
    delayMicroseconds(TR_TIME + PULSE_SHORTEN_2);
    digitalWrite(TX_PIN, HIGH);
    delayMicroseconds(TWOTR_TIME - PULSE_SHORTEN_2);
    digitalWrite(TX_PIN, LOW);
  }

  if (protocol == 3)
  {
    if (prevstate) while (time_marker + TWOTR_TIME - PULSE_SHORTEN_3 >= micros());
    else while (time_marker + TWOTR_TIME >= micros());
    time_marker += TWOTR_TIME;

    if (!prevbit && prevstate)
    {
      digitalWrite(TX_PIN, LOW);
      prevstate = 0;
      prevbit = 1;
      return;
    }
    if (!prevbit && !prevstate)
    {
      digitalWrite(TX_PIN, HIGH);
      prevstate = 1;
      prevbit = 1;
      return;
    }
    if (prevbit && prevstate)
    {
      digitalWrite(TX_PIN, LOW);
      delayMicroseconds(TR_TIME);
      digitalWrite(TX_PIN, HIGH);
      prevbit = 1;
      return;
    }
    if (prevbit && !prevstate)
    {
      digitalWrite(TX_PIN, HIGH);
      delayMicroseconds(TR_TIME);
      digitalWrite(TX_PIN, LOW);
      prevbit = 1;
      return;
    }

  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendMSB(byte data)
{
  (bitRead(data, 4)) ? sendOne() : sendZero();
  (bitRead(data, 5)) ? sendOne() : sendZero();
  (bitRead(data, 6)) ? sendOne() : sendZero();
  (bitRead(data, 7)) ? sendOne() : sendZero();
  if (protocol == 2) time_marker += timing_corrector2;       //Поправка на разницу тактовых частот 1024.07Гц и 1024.60Гц
  if (protocol == 3) time_marker += timing_corrector3;


}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendLSB(byte data)
{
  (bitRead(data, 0)) ? sendOne() : sendZero();
  (bitRead(data, 1)) ? sendOne() : sendZero();
  (bitRead(data, 2)) ? sendOne() : sendZero();
  (bitRead(data, 3)) ? sendOne() : sendZero();
  if (protocol == 2) time_marker += timing_corrector2;       //Поправка на разницу тактовых частот 1024.07Гц и 1024.60Гц
  if (protocol == 3) time_marker += timing_corrector3;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendData()
{
  int q = 0;
  for (byte i = 0; i < max_buffer_size; i++)
  {
    sendMSB(SendBuffer[i]);
    q++;
    if (q >= buffer_size) break;
    sendLSB(SendBuffer[i]);
    q++;
    if (q >= buffer_size) break;
    if (protocol == 2) time_marker += 4;       //Поправка на разницу тактовых частот 1024.07Гц и 1024.60Гц
    //if (protocol == 3) time_marker += 4;
    //Поправка на разницу тактовых частот 1024.07Гц и 1024Гц
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendOregon()
{
  time_marker = micros();
  sendPreamble();
  sendLSB(0xA);
  sendData();
  sendZero();
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void IRAM_ATTR Oregon_TM::sendPreamble(void)
{
  if (protocol == 2) {
    sendLSB(0xF);
    sendLSB(0xF);
    time_marker += 9;
    sendLSB(0xF);
    sendLSB(0xF);
    time_marker += 9;
  }
  if (protocol == 3) {
    sendLSB(0xF);
    sendLSB(0xF);
    sendLSB(0xF);
    sendLSB(0xF);
    time_marker += 4;
    sendLSB(0xF);
    sendLSB(0xF);
    time_marker += 3;
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum129(void)
{
  byte CCIT_POLY = 0x07;
  SendBuffer[9] &= 0xF0;
  SendBuffer[10] = 0x00;
  SendBuffer[11] = 0x00;
  byte summ = 0x00;
  byte crc = 0x00;
  byte cur_nible;
  for (int i = 0; i < 10; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    if (i != 3)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  SendBuffer[9] += summ & 0x0F;
  SendBuffer[10] += summ & 0xF0;
  SendBuffer[10] += crc & 0x0F;
  SendBuffer[11] += crc & 0xF0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum968(void)
{
  byte CCIT_POLY = 0x07;
  SendBuffer[9] &= 0xF0;
  SendBuffer[10] = 0x00;
  SendBuffer[11] = 0x00;
  byte summ = 0x00;
  byte crc = 0xA1;
  byte cur_nible;
  for (int i = 0; i < 10; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    if (i != 3)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  SendBuffer[9] += summ & 0x0F;
  SendBuffer[10] += summ & 0xF0;
  SendBuffer[10] += crc & 0x0F;
  SendBuffer[11] += crc & 0xF0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum132(void)
{
  byte CCIT_POLY = 0x07;
  SendBuffer[7] &= 0xF0;
  SendBuffer[8] = 0x00;
  SendBuffer[9] = 0x00;
  byte summ = 0x00;
  byte crc = 0x3C;
  byte cur_nible;
  for (int i = 0; i < 8; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    if (i != 3)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  SendBuffer[7] += summ & 0x0F;
  SendBuffer[8] += summ & 0xF0;
  SendBuffer[8] += crc & 0x0F;
  SendBuffer[9] += crc & 0xF0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum132S(void)
{
  byte CCIT_POLY = 0x07;
  byte summ = 0x00;
  byte crc = 0xD6;
  SendBuffer[6] = SendBuffer[7] = 0x00;
  byte cur_nible;
  for (int i = 0; i < 6; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    if (i != 3)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  for (int j = 0; j < 4; j++)
    if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
    else crc <<= 1;

  SendBuffer[6] += (summ & 0x0F) << 4;
  SendBuffer[6] += (summ & 0xF0) >> 4;
  SendBuffer[7] += (crc & 0x0F) << 4;
  SendBuffer[7] += (crc & 0xF0) >> 4;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum318()
{
  byte CCIT_POLY = 0x07;
  SendBuffer[7] = SendBuffer[7] & 0xF0;
  SendBuffer[8] = 0x00;
  SendBuffer[9] = 0x00;
  byte summ = 0x00;
  byte crc = 0x00;
  byte cur_nible;
  for (int i = 0; i < 8; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    if (i != 3)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  SendBuffer[7] += summ & 0x0F;
  SendBuffer[8] += summ & 0xF0;
  SendBuffer[8] += crc & 0x0F;
  SendBuffer[9] += crc & 0xF0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksum810()
{
  byte CCIT_POLY = 0x07;
  SendBuffer[7] = SendBuffer[7] & 0xF0;
  SendBuffer[8] = 0x00;
  SendBuffer[9] = 0x00;
  byte summ = 0x00;
  byte crc = 0x00;
  byte cur_nible;
  for (int i = 0; i < 8; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }
  SendBuffer[7] += summ & 0x0F;
  SendBuffer[8] += summ & 0xF0;
  SendBuffer[8] += crc & 0x0F;
  SendBuffer[9] += crc & 0xF0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::SendPacket()
{
  if (sens_type == BTHR968)
    calculateAndSetChecksum968();
  if (sens_type == BTHGN129)
    calculateAndSetChecksum129();
  if (sens_type == THGN132)
    calculateAndSetChecksum132();
  if (sens_type == THN132)
    calculateAndSetChecksum132S();
  if (sens_type == RTGN318)
    calculateAndSetChecksum318();
  if (sens_type == THGR810)
    calculateAndSetChecksum810();
  if (sens_type == THP)
    calculateAndSetChecksumTHP();
  if (sens_type == RTGR328N)
    calculateAndSetChecksumRTGR328N();
  if (sens_type == UVR128)                  // UV SENSOR (UVR128)
    calculateAndSetChecksumUVR128();

  sendOregon();
  digitalWrite(TX_PIN, LOW);
  if (protocol == 2) {
    delayMicroseconds(TWOTR_TIME * 15);
    sendOregon();
    digitalWrite(TX_PIN, LOW);
  }
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// Функции кодирования данных//////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setType(word type)
{
  sens_type = type;
  if (type == THP)
  {
    SendBuffer[0] = 0x55;
    return;
  }
  SendBuffer[0] = (type & 0xFF00) >> 8;
  SendBuffer[1] = type & 0x00FF;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setChannel(byte channel)
{
  byte channel_code;

  if (sens_type == BTHR968)
  {
    channel_code = 0x00;
    setId(0xF0);
    send_time = 40000;
  }

  if (sens_type == THGN132)
  {
    if (channel <= 1)
    {
      channel_code = 0x10;
      setId(0xE3);
      send_time = 39000;
    }
    if (channel == 2)
    {
      channel_code = 0x20;
      setId(0xE3);
      send_time = 41000;
    }
    if (channel == 3)
    {
      channel_code = 0x40;
      setId(0xBB);
      send_time = 43000;
    }
    protocol = 2;
  }

  if (sens_type == THN132)
  {
    if (channel <= 1)
    {
      channel_code = 0x10;
      setId(0xE3);
      send_time = 39000;
    }
    if (channel == 2)
    {
      channel_code = 0x20;
      setId(0xE3);
      send_time = 41000;
    }
    if (channel == 3)
    {
      channel_code = 0x40;
      setId(0xBB);
      send_time = 43000;
    }
    protocol = 2;
  }


  if (sens_type == RTGN318 || sens_type == BTHGN129)
  {

    if (channel <= 1)
    {
      channel_code = 0x10;
      setId(0xF1);
      send_time = 53000;
    }
    if (channel == 2)
    {
      channel_code = 0x20;
      setId(0x92);
      send_time = 59000;
    }
    if (channel == 3)
    {
      channel_code = 0x30;
      setId(0xAA);
      send_time = 61000;
    }

    if (channel == 4)
    {
      channel_code = 0x40;
      setId(0x8A);
      send_time = 67000;
    }

    if (channel >= 5)
    {
      channel_code = 0x50;
      setId(0xB1);
      send_time = 71000;
    }
    protocol = 2;
  }

  if (sens_type == THGR810)
  {
    if (channel <= 1)
    {
      channel_code = 0x10;
      setId(0xCB);
      send_time = 53000;
    }
    if (channel == 2)
    {
      channel_code = 0x20;
      setId(0x69);
      send_time = 59000;
    }
    if (channel == 3)
    {
      channel_code = 0x30;
      setId(0xAA);
      send_time = 61000;
    }
    if (channel == 4)
    {
      channel_code = 0x40;
      setId(0x8A);
      send_time = 67000;
    }
    if (channel == 5)
    {
      channel_code = 0x50;
      setId(0xB1);
      send_time = 71000;
    }
    if (channel == 6)
    {
      channel_code = 0x60;
      send_time = 79000;
    }
    if (channel == 7)
    {
      channel_code = 0x70;
      send_time = 83000;
    }
    if (channel == 8)
    {
      channel_code = 0x80;
      send_time = 87000;
    }
    if (channel == 9)
    {
      channel_code = 0x90;
      send_time = 91000;
    }
    if (channel >= 10)
    {
      channel_code = 0xA0;
      send_time = 93000;
    }
    protocol = 3;
  }

  // RTGR328N Clock packet - same channel coding as RTGN318, protocol v2.1
  // The RTGR328N clock packet uses sensor type 0x8CE3 (nibbles: 8, C, E, 3)
  // Channel and ID should match the corresponding RTGN318 temperature/humidity packet
  // so that the weather station associates both packets with the same sensor.
  if (sens_type == RTGR328N)
  {
    if (channel <= 1)
    {
      channel_code = 0x10;
      setId(0xF1);
      send_time = 53000;
    }
    if (channel == 2)
    {
      channel_code = 0x20;
      setId(0x92);
      send_time = 59000;
    }
    if (channel == 3)
    {
      channel_code = 0x30;
      setId(0xAA);
      send_time = 61000;
    }
    if (channel == 4)
    {
      channel_code = 0x40;
      setId(0x8A);
      send_time = 67000;
    }
    if (channel >= 5)
    {
      channel_code = 0x50;
      setId(0xB1);
      send_time = 71000;
    }
    protocol = 2;
  }

  // ── UV SENSOR (UVR128) ── BEGIN ──────────────────────────────────────────
  // Values reverse-engineered from REAL UVR128 captures (rtl_433_tests, decoded
  // here): channel nibble = 0x1 and device id = 0x96 (=150) → msg[2]=0x16, msg[3]
  // high nibble = 9. setId(0x69) reproduces that (id hi=6→msg[2] low, id lo=9→
  // msg[3] high). Nominal period 73s. The base learns the id at pairing.
  if (sens_type == UVR128)
  {
    channel_code = 0x10;
    setId(0x69);
    send_time = 73000;
    protocol = 2;
  }
  // ── UV SENSOR (UVR128) ── END ────────────────────────────────────────────

  SendBuffer[2] &= 0x0F;
  SendBuffer[2] += channel_code & 0xF0;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setId(byte ID)
{
  SendBuffer[2] &= 0xF0;
  SendBuffer[2] += (ID & 0xF0) >> 4;
  SendBuffer[3] &= 0x0F;
  SendBuffer[3] += (ID & 0x0F) << 4;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setBatteryFlag(bool level)
{
  SendBuffer[3] &= 0xFB;
  if (level) SendBuffer[3] |= 0x04;
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setStartCount(byte startcount)
{
  SendBuffer[3] &= 0xF4;
  if (startcount == 8) SendBuffer[3] |= 0x08;
  if (startcount == 2) SendBuffer[3] |= 0x02;
  if (startcount == 1) SendBuffer[3] |= 0x01;
}


///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setPressure(float mm_hg_pressure)
{
  //Ограничения датчика по даташиту
  word pressure =  (word)(mm_hg_pressure / 0.75);
  if (mm_hg_pressure < 450) pressure = 600;
  if (mm_hg_pressure > 790) pressure = 1054;

  if (sens_type == BTHR968)
  {
    pressure -=  600;
    SendBuffer[7] &= 0xF0;
    SendBuffer[7] += pressure & 0x0F;
    SendBuffer[8] = (pressure & 0x0F0) + ((pressure & 0xF00) >> 8);
    //прогноз - переменно
    SendBuffer[9] &= 0x0F;
    SendBuffer[9] += 0x60;
  }

  if (sens_type == BTHGN129)
  {
    pressure -=  545;
    SendBuffer[7] &= 0xF0;
    SendBuffer[7] += pressure & 0x0F;
    SendBuffer[8] = (pressure & 0x0F0) + ((pressure & 0xF00) >> 8);
    SendBuffer[9] &= 0x0F;
    SendBuffer[9] += 0x60;
  }

}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setTemperature(float temp)
{
  if (temp < 0)
  {
    SendBuffer[5] = 0x08;
    temp *= -1;
  }
  else
  {
    SendBuffer[5] = 0x00;
  }
  byte tempInt = (byte) temp;
  byte td = (tempInt / 10);
  byte tf = tempInt - td * 10;
  byte tempFloat = (temp - (float)tempInt) * 10;

  SendBuffer[5] += (td << 4);
  SendBuffer[4] = tf;
  SendBuffer[4] |= (tempFloat << 4);
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setHumidity(byte hum)
{
  if (sens_type != THN132)
  {
    SendBuffer[6] = (hum / 10);
    SendBuffer[6] += (hum - (SendBuffer[6] * 10)) << 4;
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setComfort(float temp, byte hum)
{
  if (sens_type != THN132)
  {
    if (hum > 70)
    {
      SendBuffer[7] = 0xC0;
      return;
    }
    if (hum < 40)
    {
      SendBuffer[7] = 0x80;
      return;
    }
    if (temp > 20 && temp < 25)
    {
      SendBuffer[7] = 0x40;
      return;
    }
    else SendBuffer[7] = 0x00;
    return;
  }
}
///////////////////////////////////////////////////////////////////////////////////////////////////

bool Oregon_TM::transmit()
{
  if (millis() >= time_marker_send && send_time)
  {
    SendPacket();
    time_marker_send = millis() + send_time;
    return true;
  }
  else return false;
}






///////////////////////////////////////////////////////////////////////////////////////////////////
//Поддержка датчика THP
///////////////////////////////////////////////////////////////////////////////////////////////////


void Oregon_TM::setChannelTHP(byte channel)
{
  SendBuffer[1] &= 0x0F;
  SendBuffer[1] += channel << 4;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setBatteryTHP( word bat_voltage)
{
  SendBuffer[6] = (bat_voltage & 0x0FF0) >> 4;
  SendBuffer[7] &= 0x0F;
  SendBuffer[7] += (bat_voltage & 0x000F) << 4;

}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setTemperatureTHP(float bme_temperature)
{
  word temp_code;
  if (bme_temperature < -100 || bme_temperature > 100) temp_code = 0x0FFF;
  else temp_code = (word)((bme_temperature + 100) * 10);
  SendBuffer[2] = temp_code & 0x00FF;
  SendBuffer[1] &= 0xF0;
  SendBuffer[1] += (temp_code & 0x0F00) >> 8;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setHumidityTHP(float bme_humidity)
{
  word hum_code;
  if (bme_humidity > 100) hum_code = 0x0FFF;
  else hum_code = (word)(bme_humidity * 10);
  SendBuffer[3] = (hum_code & 0x0FF0) >> 4;
  SendBuffer[4] &= 0x0F;
  SendBuffer[4] += (hum_code & 0x000F) << 4;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setPressureTHP(float bme_pressure)
{
  word pres_code;
  if (bme_pressure < 500) pres_code = 0x0000;
  else pres_code = (word)((bme_pressure - 500) * 10);
  SendBuffer[5] = pres_code & 0x00FF;
  SendBuffer[4] &= 0xF0;
  SendBuffer[4] += (pres_code & 0x0F00) >> 8;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::setErrorTHP()
{
  SendBuffer[1] |= 0x0F;
  SendBuffer[2] = 0xFF;
  SendBuffer[3] = 0xFF;
  SendBuffer[4] = 0xFF;
  SendBuffer[5] = 0xFF;
}
///////////////////////////////////////////////////////////////////////////////////////////////////
void Oregon_TM::calculateAndSetChecksumTHP()
{
  byte CCIT_POLY = 0x07;
  SendBuffer[7] = SendBuffer[7] & 0xF0;
  SendBuffer[8] = 0x00;
  SendBuffer[9] = 0x00;
  byte summ = 0x00;
  byte crc = 0x00;
  byte cur_nible;
  for (int i = 0; i < 8; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    crc ^= cur_nible;
    for (int j = 0; j < 4; j++)
      if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
      else crc <<= 1;

    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    crc ^= cur_nible;
    for (int j = 0; j < 4; j++)
      if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
      else crc <<= 1;
  }
  SendBuffer[7] += summ & 0x0F;
  SendBuffer[8] += summ & 0xF0;
  SendBuffer[8] += crc & 0x0F;
  SendBuffer[9] += crc & 0xF0;
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// RTGR328N Clock packet support
///////////////////////////////////////////////////////////////////////////////////////////////////
// RTGR328N Clock packet format (protocol v2.1, 25 nibbles):
//
// SendBuffer byte layout:
//   [0]    = 0x8C (sensor type high byte: first nibble=8, second nibble=C)
//   [1]    = 0xE3 (sensor type low byte: third nibble=E, fourth nibble=3)
//   [2]    = (channel_code) | (ID_high nibble)
//   [3]    = (ID_low nibble << 4) | (battery/start flags)
//   [4]    = BCD(seconds): (tens << 4) | ones
//   [5]    = BCD(minutes): (tens << 4) | ones
//   [6]    = BCD(hours):   (tens << 4) | ones
//   [7]    = BCD(day):     (tens << 4) | ones
//   [8]    = (month << 4) | weekday
//   [9]    = BCD(year):    (tens << 4) | ones (2-digit year, add 2000)
//   [10]   = (flag << 4) | checksum_low
//   [11]   = (checksum_high << 4) | CRC_low
//   [12]   = (CRC_high << 4) | 0x0
//
// Nibble layout (25 nibbles total):
//   0-3:   Sensor type (8, C, E, 3)
//   4:     Channel
//   5-6:   ID
//   7:     Battery/start flags
//   8-9:   Seconds (ones, tens) - BCD
//   10-11: Minutes (ones, tens) - BCD
//   12-13: Hours   (ones, tens) - BCD
//   14-15: Day     (ones, tens) - BCD
//   16:    Month (1-12, hex: 1-9,A,B,C)
//   17:    Weekday (0-6)
//   18-19: Year    (ones, tens) - BCD (2-digit, add 2000)
//   20:    Flag nibble (DST/timezone, set to 0)
//   21-22: Checksum (sum of nibbles 0-20, with swapped nibbles)
//   23-24: CRC8-CCITT (poly 0x07, start 0x00, ID nibbles 5-6 excluded)
///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::setClock(byte hours, byte minutes, byte seconds, byte day, byte month, int year, byte weekday)
{
  // Oregon REVERSED BCD format: (ones << 4) | tens
  // Same convention as setTemperature/setHumidity.
  // sendData() sends high nibble first (sendMSB), Oregon decoder treats
  // first nibble as the less significant digit.

  // Encode seconds: (ones << 4) | tens
  SendBuffer[4] = ((seconds % 10) << 4) | (seconds / 10);

  // Encode minutes: (ones << 4) | tens
  SendBuffer[5] = ((minutes % 10) << 4) | (minutes / 10);

  // Encode hours: (ones << 4) | tens
  SendBuffer[6] = ((hours % 10) << 4) | (hours / 10);

  // Encode day: (ones << 4) | tens
  SendBuffer[7] = ((day % 10) << 4) | (day / 10);

  // Encode month and weekday
  // Month: high nibble (1-12, hex: 1-9, A=10, B=11, C=12)
  // Weekday: low nibble (0=Sunday, 1=Monday, ... 6=Saturday)
  if (month > 12) month = 12;
  if (month < 1) month = 1;
  if (weekday > 6) weekday = 0;
  SendBuffer[8] = (month << 4) | (weekday & 0x0F);

  // Encode year: (ones << 4) | tens  (2-digit year, add 2000 when displaying)
  // NOTE: year parameter is int (not byte) to prevent overflow
  // (2026 as byte = 234 -> 234%100 = 34 instead of 26!)
  int yr = year % 100;
  SendBuffer[9] = ((yr % 10) << 4) | (yr / 10);

  // Flag nibble (nibble 20) - set to 0 (DST/timezone flag, not well documented)
  // Confirmed 2026-06-25: BAR989HG IGNORES this nibble (set 0x1 → no display change), like weekday.
  SendBuffer[10] &= 0x0F;  // Clear flag nibble (high nibble of byte 10)
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksumRTGR328N()
{
  byte CCIT_POLY = 0x07;
  // Clear checksum and CRC positions
  SendBuffer[10] &= 0xF0;   // Clear checksum low nibble (nibble 21)
  SendBuffer[11] = 0x00;    // Clear checksum high + CRC low (nibbles 22-23)
  SendBuffer[12] = 0x00;    // Clear CRC high (nibble 24)

  byte summ = 0x00;
  byte crc = 0x00;
  byte cur_nible;

  // Process bytes 0-9 (nibbles 0-19) fully
  for (int i = 0; i < 10; i++)
  {
    cur_nible = (SendBuffer[i] & 0xF0) >> 4;
    summ += cur_nible;
    // For v2.1 protocol, exclude ID nibbles from CRC:
    // ID nibble 5 = byte 2 low nibble (i=2, low)
    // ID nibble 6 = byte 3 high nibble (i=3, high)
    if (i != 3)  // byte 3 high nibble = nibble 6 (ID) → exclude from CRC
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
    cur_nible = SendBuffer[i] & 0x0F;
    summ += cur_nible;
    if (i != 2)  // byte 2 low nibble = nibble 5 (ID) → exclude from CRC
    {
      crc ^= cur_nible;
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
        else crc <<= 1;
    }
  }

  // Process byte 10 high nibble only (nibble 20 = flag nibble)
  cur_nible = (SendBuffer[10] & 0xF0) >> 4;
  summ += cur_nible;
  crc ^= cur_nible;
  for (int j = 0; j < 4; j++)
    if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
    else crc <<= 1;

  // Final CRC shift (4 bits for the last nibble)
  for (int j = 0; j < 4; j++)
    if (crc & 0x80) crc = (crc << 1) ^ CCIT_POLY;
    else crc <<= 1;

  // Place checksum: nibble 21 (low) and nibble 22 (high)
  SendBuffer[10] += summ & 0x0F;        // Checksum low nibble → byte 10 low nibble
  SendBuffer[11] += (summ & 0xF0);      // Checksum high nibble → byte 11 high nibble

  // Place CRC: nibble 23 (low) and nibble 24 (high)
  SendBuffer[11] += (crc & 0x0F);       // CRC low nibble → byte 11 low nibble
  SendBuffer[12] += (crc & 0xF0);       // CRC high nibble → byte 12 high nibble
}

///////////////////////////////////////////////////////////////////////////////////////////////////
// ── UV SENSOR (UVR128) ── BEGIN ─────────────────────────────────────────────────────────────────
///////////////////////////////////////////////////////////////////////////////////////////////////
// UVR128 frame (protocol v2.1, model 0xEC70) — REVERSE-ENGINEERED from real captures
// (rtl_433_tests/oregon_scientific/uvr128, decoded locally). 8 data bytes / 16 nibbles:
//   [0] = 0xEC (type high)
//   [1] = 0x70 (type low)
//   [2] = (channel_code 0x10) | (ID high nibble)    → real captures show 0x16 (channel=1)
//   [3] = (ID low nibble << 4) | flags              → real 0x90 (flags nibble = 0!)
//   [4] = UV index, BCD INVERTED: (units << 4) | tens   → rtl_433 get_os_uv = (lo*10)+hi
//   [5] = rolling/unused byte (varies per packet in real captures; NOT validated by
//         rtl_433 except via the checksum sum). We send a fixed value.
//   [6] = checksum (swapped nibbles of the sum of all nibbles in bytes 0-5, & 0xFF)
//   [7] = trailing byte (varies in real captures; NOT validated by rtl_433). Fixed value.
// Verified against 3 real frames: EC701690 00 FF F4 33 (uv0), ..20 8E 94 DC (uv2),
// ..60 5C 84 6A (uv6) — checksum byte6 matched all three. So buffer_size = 16.

void Oregon_TM::setUV(byte uvindex)
{
  if (uvindex > 25) uvindex = 25;       // sensor range sanity (rtl_433: 0-25)
  // BCD inverted, same convention as setTemperature/setClock:
  // low nibble = tens, high nibble = units → decoder does (lo*10)+hi.
  SendBuffer[4] = ((uvindex % 10) << 4) | (uvindex / 10);
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Oregon_TM::calculateAndSetChecksumUVR128()
{
  // ── byte 6: simple checksum ──
  // Oregon v2.1 checksum (matches rtl_433 validate_os_checksum(msg, 12)):
  // sum every nibble of bytes 0..5, mask to 0xFF, store in byte 6 with nibbles swapped.
  byte summ = 0x00;
  for (int i = 0; i < 6; i++)
    summ += (SendBuffer[i] >> 4) + (SendBuffer[i] & 0x0F);
  summ &= 0xFF;
  SendBuffer[6] = ((summ & 0x0F) << 4) | ((summ & 0xF0) >> 4);

  // ── byte 7: post-amble CRC ──
  // Per the OS RF protocol doc, the v2.1 post-amble is a CRC-8-CCITT (poly x^8+x^2+x+1
  // = 0x07) over all nibbles UP TO the checksum (nibbles 0..11 = bytes 0..5), each
  // nibble fed MSB-first. v2.1 sensors use a sensor-specific initial register value;
  // for the UVR128 it is 0xCB (reverse-engineered + verified against 7 real captures:
  // uv0/2/6/7 reproduce byte-for-byte). The CRC is stored LS-nibble-first → swapped.
  byte crc = 0xCB;
  for (int i = 0; i < 6; i++)
  {
    byte nib[2] = { (byte)(SendBuffer[i] >> 4), (byte)(SendBuffer[i] & 0x0F) };
    for (byte n = 0; n < 2; n++)
    {
      crc ^= (byte)(nib[n] << 4);
      for (int j = 0; j < 4; j++)
        if (crc & 0x80) crc = (crc << 1) ^ 0x07;
        else crc <<= 1;
    }
  }
  SendBuffer[7] = ((crc & 0x0F) << 4) | ((crc & 0xF0) >> 4);
}
///////////////////////////////////////////////////////////////////////////////////////////////////
// ── UV SENSOR (UVR128) ── END ───────────────────────────────────────────────────────────────────
///////////////////////////////////////////////////////////////////////////////////////////////////

