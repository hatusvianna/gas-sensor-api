Anesthetic Gas Sensor Communication Protocol

**General：**

Sensor interfaces to the host system via RS232 serial communication port, The serial port should be set up as follows：

Baud rate: 9600

Format:

1 start bit,

8 data bits,

no parity,

1 stop bit

**Data frames**

The Software Protocol is compatible with Phasein (Masimo).

Data is transferred between the host and sensor in frames(50ms per one frame).

A data frame consists of 21 bytes of data organized as follows:

| FLAG1 | FLAG2 | ID  | STS | Waveform data | Slow data | CHK |
| --- | --- | --- | --- | --- | --- | --- |
| 1 byte | 1 byte | 1 byte | 1 byte | 5 words (10 byte) | 6 bytes | 1 byte |

**Start of frame flags**

The sequence of FLAG1 and FLAG2, with a value of 0xAA and 0x55 respectively, is used to indicate start of frame.

**Frame ID**

The ID field identifies the slow data carried by the frame and is continuously cycled between 0 and 9 (e.g. slow data is updated with a 0.5 second interval) during all operating modes except Sleep and Selftest mode.

**Status summary**

The status summary byte STS has the following layout:

| BIT 0:7 | Name | Description | Recommended<br><br>alarm message |
| --- | --- | --- | --- |
| 0   | BDET | 1 = Breath detected | No message |
| 1   | APNEA | 1 = no breath detected within selected time | "APNEA" |
| 2   | O2_LOW | 1 = O2 sensor has lowsensitivity and will soon need to be replaced. | "O2 SENSOR LOW" |
| 3   | O2_REPL | 1 = Replace O2 sensor. | "REPLACE O2<br><br>SENSOR" |
| 4   | CHK_ADAPT | 1 = Check adapter. See<br><br>Adapter status register for<br><br>details. | "CHECK AIRWAY<br><br>ADAPTER" |
| 5   | UNSPEC_ACC | 1 = At least one parameter is<br><br>out of range. Accuracy of<br><br>current gas readings can not<br><br>be guaranteed. See Data<br><br>valid register for details. | "GAS CONC.<br><br>OUT OF RANGE" |
| 6   | SENS_ERR | 1 = Sensor error. See Sensor<br><br>error register for details. | "GAS SENSOR<br><br>ERROR" |
| 7   | O2_CALIB | 1 = Room air calibration of O2<br><br>measurement required.<br><br>See Section 5.4 for details. | "ROOM AIR<br><br>CALIBRATION<br><br>REQUIRED" |

**Waveform data** : 5 words(10 bytes)

1w：CO2 , concentration actual value\*100, send by two bytes

f = etco2_value_report_current();

usTemp = (uint16_t)(f \* 100);

pst->usCO2 High = (uint8_t)(usTemp >> 8);

pst->usCO2Low = (uint8_t)(usTemp & 0x00ff);

2w：N2O，concentration actual value\*100, send by two bytes

3w：AA1，concentration actual value\*100, send by two bytes

4w：AA2，concentration actual value\*100, send by two bytes

5w：O2，concentration actual value\*100, send by two bytes

**Slow data**

Out of 10 id bytes , 7 bytes of ID are useful, and the other 3 bytes of ID are retained. The ID value increases by 1 every 50ms, from 0X00 all the way to 0X09, and then to 0X00, that is, a 500ms loop.

| **ID** | **Name** | **Byte** | **Description** | **Value** | **Data range<sup>1</sup>** | **Note** |
| --- | --- | --- | --- | --- | --- | --- |
| 0x00 | InspVals | 0<br><br>1<br><br>2<br><br>3<br><br>4 | CO<sub>2</sub> insp. conc. N<sub>2</sub>O insp. conc. AX1 insp. conc. AX2 insp. conc.<br><br>O<sub>2</sub> insp. conc. | 0 - 250<br><br>0 - 105<br><br>0 - 250<br><br>0 - 250<br><br>0 - 105 | 0 - 25 %<br><br>0 - 105 %<br><br>0 - 25 %<br><br>0 - 25 %<br><br>0 - 105 % | Updated every breath. Switches to mom. conc. during apnea. |
| 0x01 | ExpVals | 0<br><br>1<br><br>2<br><br>3<br><br>4 | CO<sub>2</sub> exp. conc. N<sub>2</sub>O exp. conc. AX1 exp. conc. AX2 exp. conc.<br><br>O<sub>2</sub> exp. conc. | 0 - 250<br><br>0 - 105<br><br>0 - 250<br><br>0 - 250<br><br>0 - 105 | 0 - 25 %<br><br>0 - 105 %<br><br>0 - 25 %<br><br>0 - 25 %<br><br>0 - 105 % | Updated every breath. Switches to mom. conc. during apnea |
| 0x02 | MomVals | 0<br><br>1<br><br>2<br><br>3<br><br>4 | CO<sub>2</sub> momentary conc.<br><br>N<sub>2</sub>O momentary conc.<br><br>AX1 momentary conc.<br><br>AX2 momentary conc.<br><br>O<sub>2</sub> momentary conc. | 0 - 250<br><br>0 - 105<br><br>0 - 250<br><br>0 - 250<br><br>0 - 105 | 0 - 25 %<br><br>0 - 105 %<br><br>0 - 25 %<br><br>0 - 25 %<br><br>0 - 105 % | Updated every frame |
| 0x03 | GenVals | 0<br><br>1<br><br>2<br><br>3 | Respiratory rate<br><br>Time since last breath<br><br>Primary agent ID (AX1)<br><br>Secondary agent ID (AX2) | 0 - 120<br><br>1 - 255<br><br>0 - 5<br><br>0 - 5 | 0 - 120bpm<br><br>1 - 255 sec<br><br>Agent ID code<br><br>Agent ID code | Please refer to Section A.2 for details regarding Agent ID code |

|     |     | 4<br><br>5 | Atm. pressure HI byte<br><br>Atm. pressure LO byte | 500 -<br><br>1300 | 50 - 130<br><br>kPa |     |
| --- | --- | --- | --- | --- | --- | --- |
| 0x04 | SensorRegs | 0<br><br>1<br><br>2<br><br>3<br><br>4 | Sensor mode register<br><br>Reserved<br><br>Sensor error register<br><br>Adapter status register<br><br>Data valid register |     |     | Please refer to Section A.3 for sensor register details. |
| 0x05 | ConfigData | 0<br><br>1<br><br>2<br><br>3<br><br>4<br><br>5 | Sensor config. reg. 0<br><br>Sensor hw rev.<br><br>Sensor sw rev. HI byte<br><br>Sensor sw rev. LO byte<br><br>Sensor config. reg. 1<br><br>Comm protocol rev | 0-99<br><br>0-9999<br><br>0-99 | BCD coded BCD coded<br><br>BCD coded | Please refer to Section A.4 for configuration data details |
| 0x06 | ServiceData | 0<br><br>1<br><br>2<br><br>3<br><br>4<br><br>5 | Sensor s/n HI byte<br><br>Sensor s/n LO byte<br><br>Zero in progress state<br><br>Reserve<br><br>Reserve<br><br>reserve | 0-65535 | 0-65535 |     |
| 0x07 | Reserved |     |     |     |     |     |
| 0x08 | Reserved |     |     |     |     |     |
| 0x09 | Reserved |     |     |     |     |     |

Notes:Data value 255 (0xFF) is used to indicate "No data" and should be displayed as '-' on the host display

**A.2 Agent ID coding**

Anesthetic agents are coded as follows:

| Code | Agent |
| --- | --- |
| 0   | No agent |
| 1   | Halothane |
| 2   | Enflurane |
| 3   | Isoflurane |
| 4   | Sevoflurane |
| 5   | Desflurane |

**A.3 Sensor registers**

**ID 0x04, SensorRegs byte 0**

Sensor mode register has the following layout:

| Bit | Name | Description |
| --- | --- | --- |
| 2-0 | MODE | 000 = self-test mode<br><br>001 = sleep<br><br>010 = measurement<br><br>011 = demo |

**ID 0x04, SensorRegs byte 2**

Sensor error register has the following layout:

| Bit | Name | Description |
| --- | --- | --- |
| 0   | SW_ERR | 1 = Software error, Restart sensor |
| 1   | HW_ERR | 1 = Hardware error, Restart sensor |
| 2   | MFAIL | 1 = Motor speed out of bounds |
| 3   | UNCAL | 1 = Factory calibration lost/missing |

The SENS_ERR bit in the status summary byte will be set if any bit in the Sensor error register is set.

**ID 0x04, SensorRegs byte 3**

Adapter status register has the following layout:

| Bit | Name | Description |
| --- | --- | --- |
| 0   | REPL_ADAPT | 1 = Replace adapter (IR signal low) |
| 1   | NO_ADAPT | 1 = No adapter. Connect adapter(IR signal high) |
| 2   | O2_CLG | 1 = O2 port failure(clogged or plugged) |

The CHK_ADAPT bit in the status summary byte will be set if any bit in the Adapter status register is set.

**ID 0x04, SensorRegs byte 4**

Data valid register has the following layout:

| Bit | Name | Description |
| --- | --- | --- |
| **0** | CO2_OR | 1 = CO2 outside specified accuracy range \[0-10\] % |
| **1** | N2O_OR | 1 = N2O outside specified accuracy range \[0-100\] % |
| **2** | AX_OR | 1 = At least one agent outside specified accuracy range. See Section 9. |
| **3** | O2_OR | 1 = O2 outside specified accuracy range \[10-100\] % |
| **4** | TEMP_OR | 1 = Internal temperature outside operating range \[10-50\] ºC |
| **5** | PRESS_OR | 1 = Ambient pressure outside operating range \[70-120\] kPa |
| **6** | ZERO_REQ | 1 = Negative gas concentrations calculated. Zero reference calibration of IR-measurement may be required. See 5.7.2. |

The UNSPEC_ACC bit in the status summary byte will be set if any bit in the Data Valid Register is set.

The recommended alarm message for UNSPEC_ACC in section 4.2.2 is "GAS CONC. OUT OF RANGE". An even better implementation could be to use more specific messages/indications in the host instrument based on individual flags in the Data Valid register.

**ID 0x05, ConfigData byte 0**

Sensor configuration register 0 has the following layout:

| Bit | Name | Description |
| --- | --- | --- |
| **0** | O2_CFG | Oxygen option fitted |
| **1** | CO2_CFG | CO2 option fitted |
| **2** | N2O_CFG | N2O option fitted |
| **3** | HAL_CFG | Halothane option fitted |
| **4** | ENF_CFG | Enflurane option fitted |
| **5** | ISO_CFG | Isoflurane option fitted |
| **6** | SEV_CFG | Sevoflurane option fitted |
| **7** | DES_CFG | Desflurane option fitted |

**ID 0x05, ConfigData byte 4**

Sensor configuration register 1 has the following layout:

| **Bit** | **name** | **Description** |
| --- | --- | --- |
| **0** | ID_CFG | Agent ID option fitted<br><br>If this bit set 1: Agent automatic identification |

**Service status register**(ID= 0X06，ServiceData /byte 2)

| **Bit** | **name** | **Description** |
| --- | --- | --- |
| **0** | **Zero_disab** | **1：can not make zero calibration** |
| **1** | **Zero_in_progress** | **1： zero calibration in process** |
| **2** | **Span_err** | **1：Paramagnetic O2 range calibration failure** |
| **3** | **Span_calib_in_progress** | **1：Paramagnetic O2 range calibration in process** |
| **4-7** | **Ir_o2_delay** | **N/A** |

Frame checksum

The frame checksum, CHK is calculated as the two-complement of the sum over all bytes in the frame except the bytes FLAG1, FLAG2 and CHK:

uint8_t VerifyChecksum(uint8_t \*pFrm,uint8_t ucLen)

{

uint8_t ucI,ucCheckSum = 0;

for(ucI = 0; ucI < ucLen; ucI ++)

ucCheckSum += pFrm\[ucI\];

ucCheckSum = ~ucCheckSum;

ucCheckSum += 1;

return ucCheckSum;

}

Command frames from the Host to the AGM sensor consist of five bytes organized as follows:

| FLAG1 | FLAG2 | ID  | Parameter | CHK |
| --- | --- | --- | --- | --- |
| 1 byte | 1 byte | 1 byte | 1 byte | 1 byte |

Command transmission is only required in the following cases:

1.When using AGM sensor without automatic identification of anesthetic agents . The agent to be measured must be selected using the **SetPID** command.

2.When using AGM sensor without oxygen sensor. The actual oxygen concentration should be transmitted to sensor using the **SetO2** command. This will allow sensor to compensate the CO<sub>2</sub> measurement for spectral broadening effects.

3.To establish a new zero reference amplitude for the IR measurement, a room air calibration has to be performed using the **ZeroCal** command. This should be performed as a part of the preventive maintenance, or when requested by the sensor.

**The following commands are currently defined:**

| **ID** | **Name** | **Description** | **Param** | **Data range** | **Default** |
| --- | --- | --- | --- | --- | --- |
| 0x00 | SetMode | Set operating mode | 0 - 3 | 0 = Self-test<br><br>1 = Sleep mode<br><br>2 = Measurement mode<br><br>3 = Demo mode | 2   |
| 0x01 | SetApne | Set apnea time | 20-60 | 20 - 60 sec. | 20  |
| 0x02 | SetPID | Set primary agent ID if the sensor not support agent automatic identification | 0 - 5 | 0 = Not selected 1 = Halothane<br><br>2 = Enflurane<br><br>3 = Isoflurane<br><br>4 = Sevoflurane<br><br>5 = Desflurane | 0   |
| 0x03 |     | Reserved |     |     |     |
| 0x04 | SetO2 | Set O<sub>2</sub> conc. from host | 0-100,<br><br>255 | 0 -100 % set from host<br><br>255 = O<sub>2</sub><br><br>measured | 255<br><br>(oxygen option fitted)<br><br>21 (no oxygen option) |
| 0x05 |     | Reserved |     |     |     |
| 0x06 | ZeroCal | Perform zero reference calibration of **all** gas measurements (using room air with airway adapter mounted) | 255 | 255 | 255 |