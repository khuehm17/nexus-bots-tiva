#include <MotorWheel.h>

Motor::Motor(   Encoder_Params_t *encoderParams,
                unsigned char pinPWM, unsigned char pinDir_A, unsigned char pinDir_B,
			    unsigned char pinIRQ, unsigned char pinIRQB
			 )
	: PID(&speedRPMInput, &speedRPMOutput, &speedRPMDesired, KC, TAUI, TAUD),
	  m_encoderParams(encoderParams), m_pinPWM(pinPWM), m_pinDir_A(pinDir_A), m_pinDir_B(pinDir_B)
{
	m_encoderParams->pinIRQ = pinIRQ;
	m_encoderParams->pinIRQB = pinIRQB;

#if defined(__TM4C123GH6PM__)   /* For Tiva C */
	/* GPIO Pin configuration */
	pinMode(m_pinPWM, OUTPUT);
	pinMode(m_pinDir_A, OUTPUT);
    pinMode(m_pinDir_B, OUTPUT);
	/* Interrupt Pin configuration */
	pinMode(m_encoderParams->pinIRQ, INPUT_PULLUP);
#else                           /* For Arduino */
	pinMode(pinPWM, OUTPUT);
	pinMode(pinDir, OUTPUT);
	pinMode(m_encoderParams->pinIRQ, INPUT);
#endif

	if (m_encoderParams->pinIRQB != PIN_UNDEFINED)
	{
		pinMode(m_encoderParams->pinIRQB, INPUT_PULLUP);
	}
	/* Disable PID */
	PIDDisable();
}

void Motor::setupInterrupt()
{
#if defined(__TM4C123GH6PM__)   /* For Tiva C */
	attachInterrupt(m_encoderParams->pinIRQ, m_encoderParams->Encoder_Handler, RISING);
#else                           /* For Arduino */
	if (isr->pinIRQ == 2 || isr->pinIRQ == 3)
		attachInterrupt(isr->pinIRQ - 2, isr->ISRfunc, TRIGGER);
	// else
	// {
	// 	PCattachInterrupt(isr->pinIRQ, isr->ISRfunc, TRIGGER); // RISING --> CHANGE 201207
	// }
#endif
}

unsigned char Motor::getPinPWM() const
{
	return m_pinPWM;
}

unsigned char Motor::getPinDir_A() const
{
	return m_pinDir_A;
}

unsigned char Motor::getPinDir_B() const
{
	return m_pinDir_B;
}

unsigned char Motor::getPinIRQ() const
{
	return m_encoderParams->pinIRQ;
}

unsigned char Motor::getPinIRQB() const
{
	return m_encoderParams->pinIRQB;
}

void Motor::driveDirection(uint8_t dir)
{
    if (DIR_ADVANCE == dir)             // Motor turning forwards
    {
        digitalWrite(m_pinDir_A, LOW);
        digitalWrite(m_pinDir_B, HIGH);
    }
    else if (DIR_BACKOFF == dir)        // Motor turning backwards
    {
        digitalWrite(m_pinDir_A, HIGH);
        digitalWrite(m_pinDir_B, LOW);
    }
    else                                // Motor stop
    {
        digitalWrite(m_pinDir_A, LOW);
        digitalWrite(m_pinDir_B, LOW);
    }
}

unsigned int Motor::runPWM(unsigned int PWM, bool dir, bool saveDir)
{
	/* Pass PWM value to Motor::speedPWM */
	speedPWM = PWM;
	/* Save dir to Motor::desiredDirection (saving for default) */
	if (saveDir)
	{
		desiredDirection = dir;
	}

	// digitalWrite(m_pinDir_A, dir);
    // TODO: add and test driveDirection() function
    driveDirection(dir);
	/* Write PWM and direction to GPIO */
	analogWrite(m_pinPWM, PWM);
	return speedPWM;
}

unsigned int Motor::advancePWM(unsigned int PWM)
{
	return runPWM(PWM, DIR_ADVANCE);
}

unsigned int Motor::backoffPWM(unsigned int PWM)
{
	return runPWM(PWM, DIR_BACKOFF);
}

unsigned int Motor::getPWM() const
{
	return speedPWM;
}

bool Motor::setDesiredDir(bool dir)
{
	desiredDirection = dir;
	return getDesiredDir();
}

bool Motor::getDesiredDir() const
{
	return desiredDirection;
}

bool Motor::reverseDesiredDir()
{
	runPWM(getPWM(), !getDesiredDir());
	return getDesiredDir();
}

bool Motor::setCurrDir()
{
	if (getPinIRQB() != PIN_UNDEFINED)
	{
		return m_encoderParams->currDirection = digitalRead(m_encoderParams->pinIRQB);
	}
	return false;
}

bool Motor::getCurrDir() const
{
	return m_encoderParams->currDirection;
}

int Motor::getSpeedRPM() const
{
	if (getCurrDir() == DIR_ADVANCE)
	{
		return SPEEDPPS2SPEEDRPM(m_encoderParams->speedPPS);
	}
	return -SPEEDPPS2SPEEDRPM(m_encoderParams->speedPPS);
}

unsigned int Motor::setSpeedRPM(int speedRPM, bool dir)
{
	PIDSetSpeedRPMDesired(speedRPM);
	setDesiredDir(dir);
	return abs(getSpeedRPM());
}

int Motor::setSpeedRPM(int speedRPM)
{
	if (speedRPM >= 0)
	{
		return setSpeedRPM(speedRPM, DIR_ADVANCE);
	}
	else
	{
		return setSpeedRPM(abs(speedRPM), DIR_BACKOFF);
	}
}

bool Motor::PIDSetup(float kc, float taui, float taud, unsigned int sampleTime)
{  
	PID::SetTunings(kc, taui, taud);
	PID::SetInputLimits(0, MAX_SPEEDRPM);
	PID::SetOutputLimits(0, MAX_SPEEDRPM);
	PID::SetSampleTime(sampleTime);
	PID::SetMode(AUTO);
	return true;
}

bool Motor::PIDGetStatus() const
{
	return pidCtrl;
}

bool Motor::PIDEnable(float kc, float taui, float taud, unsigned int sampleTime)
{
	setupInterrupt();
	PIDSetup(kc, taui, taud, sampleTime);
	return pidCtrl = true;
}

bool Motor::PIDDisable()
{
	return pidCtrl = false;
}

bool Motor::PIDReset()
{
	if (PIDGetStatus() == false)
	{
		return false;
	}
	PID::Reset();
	return true;
}

bool Motor::PIDRegulate(bool doRegulate)
{
	if (PIDGetStatus() == false) // IF PID was disabled
	{
		return false;
	}

	if ((getPinIRQB() != PIN_UNDEFINED) && (getDesiredDir() != getCurrDir()))
	{
		speedRPMInput = - SPEEDPPS2SPEEDRPM(m_encoderParams->speedPPS);
	}
	else
	{
		speedRPMInput = SPEEDPPS2SPEEDRPM(m_encoderParams->speedPPS);
	}
    // Start PID Computation
	PID::Compute();
    // Do regulate
	if (doRegulate && PID::JustCalculated())
	{
		speed2DutyCycle += speedRPMOutput;

        // Limiting speed2DutyCycle value
		if (speed2DutyCycle >= MAX_SPEEDRPM)
		{
			speed2DutyCycle = MAX_SPEEDRPM;
		}
		else if (speed2DutyCycle <= -MAX_SPEEDRPM)
		{
			speed2DutyCycle = -MAX_SPEEDRPM;
		}

        // Run PWM 
		if (speed2DutyCycle >= 0)
		{
            // Run PWM with re-mapped speed2DutyCycle from (0-MAX_SPEEDRPM) to (0-MAX_PWM) range
			runPWM(map(speed2DutyCycle, 0, MAX_SPEEDRPM, 0, MAX_PWM), getDesiredDir(), false);
		}
		else
		{
			runPWM(map(abs(speed2DutyCycle), 0, MAX_SPEEDRPM, 0, MAX_PWM), !getDesiredDir(), false);
		}
		return true;
	}

	return false;
}

unsigned int Motor::PIDSetSpeedRPMDesired(unsigned int speedRPM)
{
	if (speedRPM > MAX_SPEEDRPM)
	{
		speedRPMDesired = MAX_SPEEDRPM;
	}
	else
	{
		speedRPMDesired = speedRPM;
	}
	return PIDGetSpeedRPMDesired();
}

unsigned int Motor::PIDGetSpeedRPMDesired() const
{
	return speedRPMDesired;
}

int Motor::getSpeedPPS() const
{
	return m_encoderParams->speedPPS;
}

long Motor::getCurrPulse() const
{
	return m_encoderParams->pulses;
}

long Motor::setCurrPulse(long _pulse)
{
	m_encoderParams->pulses = _pulse;
	return getCurrPulse();
}

int Motor::getSpeedRPMOutput()
{
    if (speed2DutyCycle >= 0)
    {
        // Run PWM with re-mapped speed2DutyCycle from (0-MAX_SPEEDRPM) to (0-MAX_PWM) range
        return map(speed2DutyCycle, 0, MAX_SPEEDRPM, 0, MAX_PWM);
    }
    else
    {
        return map(abs(speed2DutyCycle), 0, MAX_SPEEDRPM, 0, MAX_PWM);
    }
}

long Motor::resetCurrPulse()
{
	return setCurrPulse(0);
}

void Motor::delayMS(unsigned int ms, bool debug)
{
	for (unsigned long endTime = millis() + ms; millis() < endTime;)
	{
		PIDRegulate();
		if (debug && (millis() % 500 == 0))
		{
			debugger();
		}
        
		if (endTime - millis() >= SAMPLETIME)
		{
			delay(SAMPLETIME);
		}
		else
		{
			delay(endTime - millis());
		}
	}
}

void Motor::debugger() const
{
    // DEBUG_PRINTF("m_pinPWM      -> %d", m_pinPWM);
    // DEBUG_PRINTF("m_pinDir      -> %d", m_pinDir);
    // DEBUG_PRINTF("pinIRQ        -> %d", m_encoderParams->pinIRQ);
    // DEBUG_PRINTF("pinIRQB       -> %d", m_encoderParams->pinIRQB);

	DEBUG_PRINTF("DesiredDir    -> %d", desiredDirection);
	DEBUG_PRINTF("currDir       -> %d", m_encoderParams->currDirection);
	DEBUG_PRINTF("PWM           -> %d", speedPWM);
	DEBUG_PRINTF("Input         -> %d", speedRPMInput);
	DEBUG_PRINTF("Output        -> %d", speedRPMOutput);
	DEBUG_PRINTF("Desired       -> %d", speedRPMDesired);

    // DEBUG_PRINTF("speed2DutyCycle   -> %f", speed2DutyCycle);
    // DEBUG_PRINTF("speedPPS          -> %d", m_encoderParams->speedPPS);
    // DEBUG_PRINTF("pulses            -> %ld", m_encoderParams->pulses);
}

void Motor::encoderHandler(void)
{
    static bool first_pulse = true;
    m_encoderParams->pulseEndMicros = micros();
    if ((first_pulse == false) && 
        (m_encoderParams->pulseEndMicros > m_encoderParams->pulseStartMicros))
    {
        m_encoderParams->speedPPS = 
            MICROS_PER_SEC / (m_encoderParams->pulseEndMicros - m_encoderParams->pulseStartMicros);
        // encoderWheel_1_Params.accPPSS =
        // (encoderWheel_1_Params.speedPPS-encoderWheel_1_Params.lastSpeedPPS)*encoderWheel_1_Params.speedPPS;
    }
    else
    {
        first_pulse = false;
    }
    
    m_encoderParams->pulseStartMicros = m_encoderParams->pulseEndMicros;
    // encoderWheel_1_Params.lastSpeedPPS=encoderWheel_1_Params.speedPPS;
    if (m_encoderParams->pinIRQB != PIN_UNDEFINED)
    {
        m_encoderParams->currDirection = 
            DIR_INVERSE(digitalRead(m_encoderParams->pinIRQ) ^ digitalRead(m_encoderParams->pinIRQB));
    }

    // Encoder Direction detection (Tested with hardware)
    if (DIR_ADVANCE == m_encoderParams->currDirection)
    {
        ++ m_encoderParams->pulses;
    }
    else
    {
        -- m_encoderParams->pulses;
    }
}

GearedMotor::GearedMotor(unsigned char pinPWM, unsigned char pinDir_A, unsigned char pinDir_B,
						 unsigned char pinIRQ, unsigned char pinIRQB,
						 Encoder_Params_t *encoderParams, unsigned int ratio) 
            : Motor(encoderParams, pinPWM, pinDir_A, pinDir_B, pinIRQ, pinIRQB), _ratio(ratio)
{
	;
}

unsigned int GearedMotor::getRatio() const
{
	return _ratio;
}

unsigned int GearedMotor::setRatio(unsigned int ratio)
{
	_ratio = ratio;
	return getRatio();
}

float GearedMotor::getGearedSpeedRPM() const
{
	return (float)Motor::getSpeedRPM() / getRatio();
}

float GearedMotor::setGearedSpeedRPM(float gearedSpeedRPM, bool dir)
{
	// Motor::setSpeedRPM(abs(gearedSpeedRPM*REDUCTION_RATIO),dir);
	Motor::setSpeedRPM(abs(round(gearedSpeedRPM * _ratio)), dir);
	return getGearedSpeedRPM();
}

float GearedMotor::setGearedSpeedRPM(float gearedSpeedRPM)
{
	Motor::setSpeedRPM(round(gearedSpeedRPM * _ratio));
	return getGearedSpeedRPM();
}

MotorWheel::MotorWheel(unsigned char pinPWM, unsigned char pinDir_A, unsigned char pinDir_B,
					   unsigned char pinIRQ, unsigned char pinIRQB,
					   Encoder_Params_t *encoderParams,
					   unsigned int ratio, unsigned int cirMM) 
            : GearedMotor(pinPWM, pinDir_A, pinDir_B, pinIRQ, pinIRQB, encoderParams, ratio), _cirMM(cirMM)
{
	;
}
unsigned int MotorWheel::getCirMM() const
{
	return _cirMM;
}
unsigned int MotorWheel::setCirMM(unsigned int cirMM)
{
	if (cirMM > 0)
		_cirMM = cirMM;
	return getCirMM();
}

/*
int MotorWheel::getAccCMPMM() const {
	debug();
	return int(GearedMotor::getGearedAccRPMM()*getCirMM()/10);
}
 */

int MotorWheel::getSpeedCMPM() const
{
	return int(GearedMotor::getGearedSpeedRPM() * getCirMM() / 10);
}

int MotorWheel::setSpeedCMPM(unsigned int cm, bool dir)
{
	GearedMotor::setGearedSpeedRPM(cm * 10.0 / getCirMM(), dir);
	return getSpeedCMPM();
}

int MotorWheel::setSpeedCMPM(int cm)
{
	// GearedMotor::setGearedSpeedRPM(cm/CIR,dir);
	GearedMotor::setGearedSpeedRPM(cm * 10.0 / getCirMM());
	return getSpeedCMPM();
}

/*
int MotorWheel::getAccMMPSS() const {
	debug();
	return int(getAccCMPMM()/6);
}
 */

int MotorWheel::getSpeedMMPS() const
{
	return int(getSpeedCMPM() / 6); //(mm/sec)/(cm/min) = 6
}

int MotorWheel::setSpeedMMPS(unsigned int mm, bool dir)
{
	setSpeedCMPM(mm * 6, dir);
	return getSpeedMMPS();
}

// direction sensitive, 201208
int MotorWheel::setSpeedMMPS(int mm)
{
	setSpeedCMPM(mm * 6);
	return getSpeedMMPS();
}
