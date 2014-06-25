/*----------------------------------------------------------------------------*/
/* Copyright (c) FIRST 2014. All Rights Reserved.                             */
/* Open Source Software - may be modified and shared by FRC teams. The code   */
/* must be accompanied by the FIRST BSD license file in the root directory of */
/* the project.                                                               */
/*----------------------------------------------------------------------------*/

#include "WPILib.h"
#include "gtest/gtest.h"
#include "TestBench.h"

static constexpr double kMotorTime = 0.5;

static constexpr double kEncoderSettlingTime = 0.25;
static constexpr double kEncoderPositionTolerance = 5.0/360.0; // +/-5 degrees

static constexpr double kPotentiometerSettlingTime = 0.05;
static constexpr double kPotentiometerPositionTolerance = 10.0/360.0; // +/-10 degrees

// TODO test coverage for CANJaguar
class CANJaguarTest : public testing::Test {
protected:
    CANJaguar *m_jaguar;
    DigitalOutput *m_fakeForwardLimit, *m_fakeReverseLimit;
    AnalogOutput *m_fakePotentiometer;

    double m_initialPosition;

    virtual void SetUp() {
        m_jaguar = new CANJaguar(TestBench::kCANJaguarID);
        m_jaguar->ChangeControlMode(CANJaguar::kPercentVbus);
        m_jaguar->SetPositionReference(CANJaguar::kPosRef_QuadEncoder);
        m_jaguar->ConfigEncoderCodesPerRev(360);

        m_fakeForwardLimit = new DigitalOutput(TestBench::kFakeJaguarForwardLimit);
        m_fakeForwardLimit->Set(0);

        m_fakeReverseLimit = new DigitalOutput(TestBench::kFakeJaguarReverseLimit);
        m_fakeReverseLimit->Set(0);

        m_fakePotentiometer = new AnalogOutput(TestBench::kFakeJaguarPotentiometer);
        m_fakePotentiometer->SetVoltage(0.0f);

        /* The motor might still have momentum from the previous test. */
        Wait(kEncoderSettlingTime);
        m_initialPosition = m_jaguar->GetPosition();
    }

    virtual void TearDown() {
        delete m_jaguar;
        delete m_fakeForwardLimit;
        delete m_fakeReverseLimit;
        delete m_fakePotentiometer;
    }
};

/**
 * Test if we can drive the motor in percentage mode and get a position back
 */
TEST_F(CANJaguarTest, PercentForwards) {
    /* Drive the speed controller briefly to move the encoder */
    m_jaguar->Set(1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should have increased */
    EXPECT_GT(m_jaguar->GetPosition(), m_initialPosition)
        << "CAN Jaguar position should have increased after the motor moved";
}

/**
 * Test if we can drive the motor backwards in percentage mode and get a
 * position back
 */
TEST_F(CANJaguarTest, PercentReverse) {
    /* Drive the speed controller briefly to move the encoder */
    m_jaguar->Set(-1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should have decreased */
    EXPECT_LT(m_jaguar->GetPosition(), m_initialPosition)
        << "CAN Jaguar position should have decreased after the motor moved";
}

/**
 * Test if we can set a position and reach that position with PID control on
 * the Jaguar.
 */
TEST_F(CANJaguarTest, EncoderPositionPID) {
    m_jaguar->ChangeControlMode(CANJaguar::kPosition);
    m_jaguar->SetPID(5.0f, 0.1f, 2.0f);
    m_jaguar->EnableControl();

    double setpoint = m_initialPosition + 10.0f;

    /* It should get to the setpoint within 5 seconds */
    m_jaguar->Set(setpoint);
    Wait(5.0f);

    EXPECT_NEAR(setpoint, m_jaguar->GetPosition(), kEncoderPositionTolerance)
        << "CAN Jaguar should have reached setpoint with PID control";
}

/**
 * Test if we can get a position in potentiometer mode, using an analog output
 * as a fake potentiometer.
 */
TEST_F(CANJaguarTest, FakePotentiometerPosition) {
    m_jaguar->SetPositionReference(CANJaguar::kPosRef_Potentiometer);
    m_jaguar->ConfigPotentiometerTurns(1);
    m_jaguar->EnableControl();

    m_fakePotentiometer->SetVoltage(0.0f);
    Wait(kPotentiometerSettlingTime);
    EXPECT_NEAR(m_fakePotentiometer->GetVoltage() / 3.0f, m_jaguar->GetPosition(), kPotentiometerPositionTolerance)
        << "CAN Jaguar should have returned the potentiometer position set by the analog output";

    m_fakePotentiometer->SetVoltage(1.0f);
    Wait(kPotentiometerSettlingTime);
    EXPECT_NEAR(m_fakePotentiometer->GetVoltage() / 3.0f, m_jaguar->GetPosition(), kPotentiometerPositionTolerance)
        << "CAN Jaguar should have returned the potentiometer position set by the analog output";

    m_fakePotentiometer->SetVoltage(2.0f);
    Wait(kPotentiometerSettlingTime);
    EXPECT_NEAR(m_fakePotentiometer->GetVoltage() / 3.0f, m_jaguar->GetPosition(), kPotentiometerPositionTolerance)
        << "CAN Jaguar should have returned the potentiometer position set by the analog output";

    m_fakePotentiometer->SetVoltage(3.0f);
    Wait(kPotentiometerSettlingTime);
    EXPECT_NEAR(m_fakePotentiometer->GetVoltage() / 3.0f, m_jaguar->GetPosition(), kPotentiometerPositionTolerance)
        << "CAN Jaguar should have returned the potentiometer position set by the analog output";
}

/**
 * Test if we can limit the Jaguar to only moving in reverse with a fake
 * limit switch.
 */
TEST_F(CANJaguarTest, FakeLimitSwitchForwards) {
    m_jaguar->ConfigLimitMode(CANJaguar::kLimitMode_SwitchInputsOnly);
    m_fakeForwardLimit->Set(1);
    m_fakeReverseLimit->Set(0);

    /* Drive the speed controller briefly to move the encoder.  If the limit
         switch is recognized, it shouldn't actually move. */
    m_jaguar->Set(1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should be the same, since the limit switch was on. */
    EXPECT_NEAR(m_initialPosition, m_jaguar->GetPosition(), kEncoderPositionTolerance)
        << "CAN Jaguar should not have moved with the limit switch pressed";

    Wait(kEncoderSettlingTime);

    /* Drive the speed controller in the other direction.  It should actually
         move, since only the forward switch is activated.*/
    m_jaguar->Set(-1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should have decreased */
    EXPECT_LT(m_jaguar->GetPosition(), m_initialPosition)
        << "CAN Jaguar should have moved in reverse while the forward limit was on";
}

/**
 * Test if we can limit the Jaguar to only moving forwards with a fake limit
 * switch.
 */
TEST_F(CANJaguarTest, FakeLimitSwitchReverse) {
    m_jaguar->ConfigLimitMode(CANJaguar::kLimitMode_SwitchInputsOnly);
    m_fakeForwardLimit->Set(0);
    m_fakeReverseLimit->Set(1);

    /* Drive the speed controller backwards briefly to move the encoder.  If
         the limit switch is recognized, it shouldn't actually move. */
    m_jaguar->Set(-1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should be the same, since the limit switch was on. */
    EXPECT_NEAR(m_initialPosition, m_jaguar->GetPosition(), kEncoderPositionTolerance)
        << "CAN Jaguar should not have moved with the limit switch pressed";

    Wait(kEncoderSettlingTime);

    /* Drive the speed controller in the other direction.  It should actually
         move, since only the reverse switch is activated.*/
    m_jaguar->Set(1.0f);
    Wait(kMotorTime);
    m_jaguar->Set(0.0f);

    /* The position should have increased */
    EXPECT_GT(m_jaguar->GetPosition(), m_initialPosition)
        << "CAN Jaguar should have moved forwards while the reverse limit was on";
}
