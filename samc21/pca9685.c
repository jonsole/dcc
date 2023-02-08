/*
 * pca9685.c
 *
 * Created: 15/12/2021 10:41:03
 *  Author: jonso
 */ 
void PCA9685_Init(void)
{
	/* Initialise I2C Master on SERCOM3, enable strong pull-ups on PIOs */
	SERCOM_I2cMasterInit(3, 400);
	PIO_EnablePullUp(PIN_PA22);	PIO_SetStrongDrive(PIN_PA22); PIO_EnableInput(PIN_PA22);
	PIO_EnablePullUp(PIN_PA23);	PIO_SetStrongDrive(PIN_PA23); PIO_EnableInput(PIN_PA23);
	PIO_SetPeripheral(PIN_PA22C_SERCOM3_PAD0, PIO_PERIPHERAL_C); PIO_EnablePeripheral(PIN_PA22C_SERCOM3_PAD0);
	PIO_SetPeripheral(PIN_PA23C_SERCOM3_PAD1, PIO_PERIPHERAL_C); PIO_EnablePeripheral(PIN_PA23C_SERCOM3_PAD1);

    /* Select slave device, select MODE1 register, then set AI (auto-increment)
       enable, SLEEP active, and ALLCALL enable */
	uint8_t InitTable1[] = { 0x00, 0b00110001};
	SERCOM_I2cMasterWrite(3, 0x41, InitTable1, sizeof(InitTable1));
	

	/* Select PRESCALE register, then set output driver frequency using prescalar */
	uint8_t InitTable2[] = { 0xFE, 121};	// 25000000 / (4096 * 50Hz) - 1	
	SERCOM_I2cMasterWrite(3, 0x41, InitTable2, sizeof(InitTable2));

    /* Select slave device, select MODE1 register, then set AI (auto-increment)
       enable, SLEEP disable, and ALLCALL enable. Select MODE2 register, then set INVRT (inverted
		output) disable, OCH (outputs change on STOP command) enable, OUTDRV
		(output driver configuration) to totem pole output and OUTNE (output not
		enable mode) to 0x00 */
	uint8_t InitTable3[] = { 0x00, 0b00100001};
	SERCOM_I2cMasterWrite(3, 0x41, InitTable3, sizeof(InitTable3));
	
	uint8_t InitTable4[] = { 0x01, 0b00001100};
	SERCOM_I2cMasterWrite(3, 0x41, InitTable4, sizeof(InitTable4));
}

#define NEUTRAL_PULSE 303	// 1500uS
#define MAX_PULSE 404	// 2000uS
#define MIN_PULSE 202  // 1000uS

void PCA9685_SetServoRaw(uint8_t Servo, uint16_t Count)
{
    uint8_t OffLowCmd = Count;
    uint8_t OffHighCmd = Count >> 8;
	uint8_t Command[] = {6 + (4 * Servo), 0x00, 0x00, OffLowCmd, OffHighCmd};
	SERCOM_I2cMasterWrite(3, 0x41, Command, sizeof(Command));
}

void PCA9685_SetServo(uint8_t Servo, int8_t Angle)
{
	/* Set limits on angle (-90 to 90 degrees) */
    if (Angle > 90)
        Angle = 90;
    else if (Angle < -90)
        Angle = -90;

    /* Calculate the pulse duration */
    uint16_t Pulse = NEUTRAL_PULSE + Angle * (MAX_PULSE - MIN_PULSE) / (180);

    /* Output turns on at 0 counts (simplest way), and will turn off according
       to calculations above. Break the 12-bit count into two 8-bit values */
    uint8_t OffLowCmd = Pulse;
    uint8_t OffHighCmd = Pulse >> 8;

    /* Each output is controlled by 2x 12-bit registers: ON to specify the count
       time to turn on the LED (a number from 0-4095), and OFF to specify the
       count time to turn off the LED (a number from 0-4095). Each 12-bit
       register is composed of 2 8-bit registers: a high and low. */

    /* Select slave device, select LEDXX_ON_L register, set contents of
       LEDXX_ON_L, then set contents of next 3 registers in sequence (only if
       auto-increment is enabled). */
	uint8_t Command[] = {6 + (4 * Servo), 0x00, 0x00, OffLowCmd, OffHighCmd};		
	SERCOM_I2cMasterWrite(3, 0x41, Command, sizeof(Command));
}
